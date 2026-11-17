#include "render/BlackHoleRenderer.hpp"

#include "physics/Blackbody.hpp"
#include "render/FrameUniforms.hpp"
#include "render/GpuPass.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace bh {

namespace {

constexpr int workGroupSize = 8; // local_size_x/y of every compute shader
constexpr std::size_t maxBloomLevels = 6;
constexpr int minBloomLevelSize = 4;
constexpr float bloomFilterRadius = 1.0F;

// Packed 32-bit float RGB (no sign, 6/6/5-bit mantissas): half the bandwidth of rgba16f, which
// dominates the bloom and present passes. Must match the image format qualifiers in the shaders.
constexpr GLenum hdrFormat = GL_R11F_G11F_B10F;

constexpr GLuint radianceUnit = 0;
constexpr GLuint exposureBinding = 1; // layout(std430, binding = 1) in exposure.comp and present.frag
// Accumulated radiance: 32-bit floats, as the running mean adds increments of 1/n of itself.
constexpr GLenum accumulationFormat = GL_RGBA32F;

// Auto exposure: the metered luminance that maps to exposure 1, chosen so the default scene looks as it
// does at a fixed 0 EV; and the adaptation time constant.
constexpr float exposureKey = 0.7F;
constexpr float exposureAdaptationSeconds = 0.4F;
// Metering samples a mip level about this many texels wide.
constexpr float meterTexels = 64.0F;
constexpr GLuint bloomUnit = 1;
constexpr GLuint diskProfileUnit = 2;  // layout(binding = 2) in common/disk.glsl
constexpr GLuint blackbodyUnit = 3;    // layout(binding = 3)
constexpr GLuint skyEquirectangularUnit = 4; // layout(binding = 4) in common/sky.glsl
constexpr GLuint skyCubeUnit = 5;            // layout(binding = 5)

// std140 rounds a block's size up to a multiple of 16 bytes; the buffer must cover all of it.
constexpr GLsizeiptr frameUniformBufferSize = (sizeof(FrameUniforms) + 15) / 16 * 16;

// Black-body look-up: log10 T from 2.5 (316 K, invisible) to 6 (1e6 K, Rayleigh-Jeans), 1024 samples.
constexpr float blackbodyLogTMin = 2.5F;
constexpr float blackbodyLogTMax = 6.0F;
constexpr int blackbodySamples = 1024;

void setLinearClamp(const GLenum target)
{
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
}

[[nodiscard]] gl::Texture createBlackbodyLut()
{
    std::vector<float> rgb {};
    rgb.reserve(3 * blackbodySamples);
    for (int i {0}; i < blackbodySamples; ++i) {
        const double logT {static_cast<double>(blackbodyLogTMin)
                           + static_cast<double>(blackbodyLogTMax - blackbodyLogTMin) * i / (blackbodySamples - 1)};
        const auto [r, g, b] {blackbodyRgb(std::pow(10.0, logT))};
        rgb.insert(rgb.end(), {static_cast<float>(r), static_cast<float>(g), static_cast<float>(b)});
    }
    gl::Texture texture {gl::createTexture()};
    glBindTexture(GL_TEXTURE_1D, texture.get());
    glTexStorage1D(GL_TEXTURE_1D, 1, GL_RGB32F, blackbodySamples);
    glTexSubImage1D(GL_TEXTURE_1D, 0, 0, blackbodySamples, GL_RGB, GL_FLOAT, rgb.data());
    setLinearClamp(GL_TEXTURE_1D);
    return texture;
}

// Largest anisotropy for texture filtering, 1 if unsupported. Core only since OpenGL 4.6; before that
// the ARB or EXT extension (same enums). Lensing stretches the sky very unevenly, so it matters here.
[[nodiscard]] float maxAnisotropy()
{
    GLint count {};
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    const bool supported {GLAD_GL_VERSION_4_6 != 0
                          || std::ranges::any_of(std::views::iota(0, count), [](const GLint i) {
                                 // glGetStringi returns const GLubyte*, a NUL-terminated character string.
                                 const std::string_view name {reinterpret_cast<const char*>(
                                     glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)))};
                                 return name == "GL_ARB_texture_filter_anisotropic"
                                     || name == "GL_EXT_texture_filter_anisotropic";
                             })};
    GLfloat value {1.0F};
    if (supported) {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &value);
    }
    return value;
}

// Newest modification time of any file below dir; errors count as "nothing changed".
[[nodiscard]] std::filesystem::file_time_type newestModification(const std::filesystem::path& dir)
{
    // Not file_time_type {}: libstdc++'s file clock has its epoch in 2174, so real times are negative.
    std::filesystem::file_time_type newest {std::filesystem::file_time_type::min()};
    std::error_code error {};
    for (std::filesystem::recursive_directory_iterator it {dir, error}, end {}; !error && it != end;
         it.increment(error)) {
        if (it->is_regular_file(error)) {
            newest = std::max(newest, it->last_write_time(error));
        }
    }
    return newest;
}

// Halton low-discrepancy sequence in base b: well-spread sub-pixel offsets for any number of samples.
[[nodiscard]] float halton(std::uint32_t index, const std::uint32_t base)
{
    float result {0.0F};
    float fraction {1.0F};
    while (index > 0U) {
        fraction /= static_cast<float>(base);
        result += fraction * static_cast<float>(index % base);
        index /= base;
    }
    return result;
}

// Sub-pixel offset of accumulated sample n: the pixel centre first, then Halton (2, 3).
[[nodiscard]] glm::vec2 jitter(const std::uint32_t sample)
{
    return sample == 0U ? glm::vec2 {0.0F} : glm::vec2 {halton(sample, 2) - 0.5F, halton(sample, 3) - 0.5F};
}

void dispatchOver(const glm::ivec2 size)
{
    glDispatchCompute(static_cast<GLuint>((size.x + workGroupSize - 1) / workGroupSize),
                      static_cast<GLuint>((size.y + workGroupSize - 1) / workGroupSize), 1);
}

void bindTexture(const GLuint unit, const GLuint texture)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
}

// Makes image writes visible to the next dispatch's image loads and texture fetches.
void imageWriteBarrier()
{
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

} // namespace

BlackHoleRenderer::BlackHoleRenderer(std::filesystem::path shaderDir)
    : m_shaderDir {std::move(shaderDir)}
    , m_shaderSnapshot {newestModification(m_shaderDir)}
    , m_programs {[this] {
        auto programs {loadPrograms()};
        if (!programs) {
            throw std::runtime_error {"shader compilation failed:\n" + programs.error()};
        }
        return std::move(*programs);
    }()}
    , m_blackbodyLut {createBlackbodyLut()}
    , m_maxAnisotropy {maxAnisotropy()}
{
    glBindBuffer(GL_UNIFORM_BUFFER, m_frameUniforms.get());
    glBufferData(GL_UNIFORM_BUFFER, frameUniformBufferSize, nullptr, GL_DYNAMIC_DRAW);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS); // filter across cube faces (the sky map)
    glBindBufferBase(GL_UNIFORM_BUFFER, frameUniformBinding, m_frameUniforms.get());

    const std::array<float, 2> exposure {0.0F, 0.0F}; // log2 exposure, metered luminance
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_exposure.get());
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(exposure), exposure.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, exposureBinding, m_exposure.get());
}

auto BlackHoleRenderer::loadPrograms() const -> std::expected<Programs, std::string>
{
    const auto load = [](const std::initializer_list<gl::ShaderFile> files) {
        auto program {gl::ShaderProgram::fromFiles(std::span {files.begin(), files.size()})};
        if (!program) {
            program = std::unexpected {files.begin()->path.filename().string() + ": " + program.error()};
        }
        return program;
    };
    using enum gl::ShaderStage;

    auto trace {load({{COMPUTE, m_shaderDir / "trace.comp"}})};
    auto downsample {load({{COMPUTE, m_shaderDir / "bloom_downsample.comp"}})};
    auto upsample {load({{COMPUTE, m_shaderDir / "bloom_upsample.comp"}})};
    auto present {load({{VERTEX, m_shaderDir / "fullscreen.vert"}, {FRAGMENT, m_shaderDir / "present.frag"}})};
    auto exposure {load({{COMPUTE, m_shaderDir / "exposure.comp"}})};
    for (const auto* const program : {&trace, &downsample, &upsample, &present, &exposure}) {
        if (!*program) {
            return std::unexpected {program->error()};
        }
    }
    return Programs {
        .trace = std::move(*trace),
        .bloomDownsample = std::move(*downsample),
        .bloomUpsample = std::move(*upsample),
        .present = std::move(*present),
        .exposure = std::move(*exposure),
    };
}

std::expected<void, std::string> BlackHoleRenderer::reloadShaders()
{
    m_shaderSnapshot = newestModification(m_shaderDir); // also after a failure: retry on the next save only
    auto programs {loadPrograms()};
    if (!programs) {
        return std::unexpected {std::move(programs.error())};
    }
    m_programs = std::move(*programs);
    m_traced.reset(); // the trace may have changed: start the accumulation over
    return {};
}

bool BlackHoleRenderer::shaderSourcesChanged() const
{
    return newestModification(m_shaderDir) > m_shaderSnapshot;
}

void BlackHoleRenderer::setDiskProfile(const std::span<const float> profile)
{
    const int size {static_cast<int>(profile.size())};
    if (size != m_diskProfileSize) {
        m_diskProfile = gl::createTexture();
        glBindTexture(GL_TEXTURE_1D, m_diskProfile.get());
        glTexStorage1D(GL_TEXTURE_1D, 1, GL_R32F, size);
        setLinearClamp(GL_TEXTURE_1D);
        m_diskProfileSize = size;
    }
    glBindTexture(GL_TEXTURE_1D, m_diskProfile.get());
    glTexSubImage1D(GL_TEXTURE_1D, 0, 0, size, GL_RED, GL_FLOAT, profile.data());
    m_traced.reset();
}

std::expected<void, std::string> BlackHoleRenderer::setSkyImage(const SkyImage& image)
{
    const SkyFace& first {image.faces.front()};
    const bool cube {image.projection == SkyProjection::CUBEMAP};
    const GLenum target {static_cast<GLenum>(cube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D)};
    GLint maxSize {};
    glGetIntegerv(cube ? GL_MAX_CUBE_MAP_TEXTURE_SIZE : GL_MAX_TEXTURE_SIZE, &maxSize);
    if (std::max(first.width, first.height) > maxSize) {
        return std::unexpected {
            std::format("{}x{} is larger than this GPU's limit of {}", first.width, first.height, maxSize)};
    }

    // HDR maps as packed floats (half the memory of rgba16f, ample for radiance); 8-bit maps stay
    // sRGB-encoded and are decoded to linear by the texture unit.
    const GLenum format {first.isHdr() ? hdrFormat : GL_SRGB8_ALPHA8};
    const int levels {std::bit_width(static_cast<unsigned>(std::max(first.width, first.height)))};
    gl::Texture texture {gl::createTexture()};
    glBindTexture(target, texture.get());
    glTexStorage2D(target, levels, format, first.width, first.height);
    for (std::size_t face {0}; face < image.faces.size(); ++face) {
        const SkyFace& source {image.faces[face]};
        const GLenum faceTarget {cube ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(face) : GL_TEXTURE_2D};
        if (const auto* const rgb {std::get_if<std::vector<float>>(&source.pixels)}) {
            glTexSubImage2D(faceTarget, 0, 0, 0, source.width, source.height, GL_RGB, GL_FLOAT, rgb->data());
        } else {
            glTexSubImage2D(faceTarget, 0, 0, 0, source.width, source.height, GL_RGBA, GL_UNSIGNED_BYTE,
                            std::get<std::vector<std::uint8_t>>(source.pixels).data());
        }
    }
    glGenerateMipmap(target);
    if (glGetError() == GL_OUT_OF_MEMORY) {
        return std::unexpected {std::format("{}x{}: out of GPU memory", first.width, first.height)};
    }
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, cube ? GL_CLAMP_TO_EDGE : GL_REPEAT); // phi wraps around
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (m_maxAnisotropy > 1.0F) {
        glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY, m_maxAnisotropy);
    }
    m_skyTexture = std::move(texture);
    m_skyProjection = image.projection;
    m_traced.reset();
    return {};
}

void BlackHoleRenderer::resizeTargets(const glm::ivec2 traceSize)
{
    const auto createTarget = [](const glm::ivec2 size) {
        gl::Texture texture {gl::createTexture()};
        glBindTexture(GL_TEXTURE_2D, texture.get());
        glTexStorage2D(GL_TEXTURE_2D, 1, hdrFormat, size.x, size.y);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return RenderTarget {.texture = std::move(texture), .size = size};
    };

    // The shown radiance has mipmaps for the metering's coarse samples (mipmapped minification); everything
    // else reads level 0.
    gl::Texture radiance {gl::createTexture()};
    glBindTexture(GL_TEXTURE_2D, radiance.get());
    glTexStorage2D(GL_TEXTURE_2D, std::bit_width(static_cast<unsigned>(std::max(traceSize.x, traceSize.y))),
                   hdrFormat, traceSize.x, traceSize.y);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_radiance = RenderTarget {.texture = std::move(radiance), .size = traceSize};

    gl::Texture accumulated {gl::createTexture()};
    glBindTexture(GL_TEXTURE_2D, accumulated.get());
    glTexStorage2D(GL_TEXTURE_2D, 1, accumulationFormat, traceSize.x, traceSize.y);
    m_accumulated = RenderTarget {.texture = std::move(accumulated), .size = traceSize};
    m_traced.reset();

    m_bloomLevels.clear();
    for (glm::ivec2 size {traceSize / 2};
         m_bloomLevels.size() < maxBloomLevels && std::min(size.x, size.y) >= minBloomLevelSize; size /= 2) {
        m_bloomLevels.push_back(createTarget(size));
    }
}

FrameUniforms BlackHoleRenderer::frameUniforms(const FrameParams& frame) const
{
    return FrameUniforms {
        .cameraFrame = frame.cameraFrame,
        .cameraPosition = glm::vec4 {frame.cameraPosition, 1.0F},
        .resolution = glm::vec2 {m_radiance.size},
        .tanHalfFov = frame.tanHalfFov,
        .time = frame.time,
        .spin = frame.spin,
        .captureRadius = frame.captureRadius,
        .diskInnerRadius = frame.diskInnerRadius,
        .diskOuterRadius = frame.diskOuterRadius,
        .stepScale = frame.stepScale,
        .maxStepFraction = frame.maxStepFraction,
        .escapeRadius = frame.escapeRadius,
        .maxSteps = static_cast<std::uint32_t>(std::max(frame.maxSteps, 1)),
        .flags = (frame.diskEnabled ? frameFlagDisk : 0U) | (frame.dopplerShift ? frameFlagDoppler : 0U)
               | (frame.limbDarkening ? frameFlagLimbDarkening : 0U)
               | (frame.gravitationalRedshift ? frameFlagGravitationalRedshift : 0U),
        .debugView = static_cast<std::uint32_t>(std::to_underlying(frame.debugView)),
        .diskPeakTemperature = frame.diskPeakTemperature,
        .diskBrightness = frame.diskBrightness,
        .diskOrbitSign = frame.diskOrbitSign,
        .blackbodyLogTMin = blackbodyLogTMin,
        .blackbodyLogTMax = blackbodyLogTMax,
        .sky = static_cast<std::uint32_t>(std::to_underlying(availableSky(frame.sky))),
        .skyRotation = glm::mat4 {frame.skyRotation},
        .skyBrightness = frame.skyBrightness,
        .diskTurbulence = frame.diskTurbulence,
        .diskEddySize = frame.diskEddySize,
        .diskEdgeSoftness = frame.diskEdgeSoftness,
    };
}

Sky BlackHoleRenderer::availableSky(const Sky requested) const
{
    if ((requested == Sky::EQUIRECTANGULAR && m_skyProjection != SkyProjection::EQUIRECTANGULAR)
        || (requested == Sky::CUBEMAP && m_skyProjection != SkyProjection::CUBEMAP)) {
        return Sky::NONE;
    }
    return requested;
}

void BlackHoleRenderer::trace(const glm::vec2 jitter, const bool accumulate)
{
    m_programs.trace.use();
    m_programs.trace.set("uJitter", jitter);
    m_programs.trace.set("uSampleIndex", m_samples);
    m_programs.trace.set("uAccumulate", accumulate);
    glBindImageTexture(1, m_accumulated.texture.get(), 0, GL_FALSE, 0, GL_READ_WRITE, accumulationFormat);
    if (m_skyProjection) {
        const bool cube {m_skyProjection == SkyProjection::CUBEMAP};
        glActiveTexture(GL_TEXTURE0 + (cube ? skyCubeUnit : skyEquirectangularUnit));
        glBindTexture(cube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D, m_skyTexture.get());
    }
    glActiveTexture(GL_TEXTURE0 + diskProfileUnit);
    glBindTexture(GL_TEXTURE_1D, m_diskProfile.get());
    glActiveTexture(GL_TEXTURE0 + blackbodyUnit);
    glBindTexture(GL_TEXTURE_1D, m_blackbodyLut.get());
    glBindImageTexture(0, m_radiance.texture.get(), 0, GL_FALSE, 0, GL_WRITE_ONLY, hdrFormat);
    dispatchOver(m_radiance.size);
    imageWriteBarrier();
}

void BlackHoleRenderer::meterExposure(const FrameParams& frame)
{
    glBindTexture(GL_TEXTURE_2D, m_radiance.texture.get());
    glGenerateMipmap(GL_TEXTURE_2D);
    gl::ShaderProgram& program {m_programs.exposure};
    program.use();
    const float width {static_cast<float>(std::max(m_radiance.size.x, m_radiance.size.y))};
    program.set("uMeterLod", std::max(std::log2(width / meterTexels), 0.0F));
    program.set("uKey", exposureKey);
    program.set("uDeltaSeconds", frame.frameSeconds);
    program.set("uAdaptationSeconds", exposureAdaptationSeconds);
    program.set("uReset", !m_exposureMetered);
    bindTexture(0, m_radiance.texture.get());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, exposureBinding, m_exposure.get());
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    m_exposureMetered = true;
}

void BlackHoleRenderer::bloom()
{
    gl::ShaderProgram& downsample {m_programs.bloomDownsample};
    downsample.use();
    for (std::size_t level {0}; level < m_bloomLevels.size(); ++level) {
        const RenderTarget& source {level == 0 ? m_radiance : m_bloomLevels[level - 1]};
        bindTexture(0, source.texture.get());
        downsample.set("uKarisAverage", level == 0);
        glBindImageTexture(0, m_bloomLevels[level].texture.get(), 0, GL_FALSE, 0, GL_WRITE_ONLY, hdrFormat);
        dispatchOver(m_bloomLevels[level].size);
        imageWriteBarrier();
    }

    gl::ShaderProgram& upsample {m_programs.bloomUpsample};
    upsample.use();
    upsample.set("uFilterRadius", bloomFilterRadius);
    for (std::size_t level {m_bloomLevels.size() - 1}; level > 0; --level) {
        bindTexture(0, m_bloomLevels[level].texture.get());
        glBindImageTexture(0, m_bloomLevels[level - 1].texture.get(), 0, GL_FALSE, 0, GL_READ_WRITE, hdrFormat);
        dispatchOver(m_bloomLevels[level - 1].size);
        imageWriteBarrier();
    }
}

void BlackHoleRenderer::present(const FrameParams& frame, const GLuint framebuffer)
{
    // Debug views show their false colours as they are: no bloom, exposure or tone curve.
    const bool debug {frame.debugView != DebugView::NONE};
    const bool bloomEnabled {frame.bloomEnabled && !debug && !m_bloomLevels.empty()};
    gl::ShaderProgram& program {m_programs.present};
    program.set("uOutputSize", glm::vec2 {frame.outputSize});
    program.set("uExposure", debug ? 1.0F : frame.exposure);
    program.set("uToneMapper", std::to_underlying(debug ? ToneMapper::CLAMP : frame.toneMapper));
    program.set("uBloomEnabled", bloomEnabled);
    program.set("uBloomStrength", frame.bloomStrength);
    program.set("uBloomNormalization", bloomEnabled ? 1.0F / static_cast<float>(m_bloomLevels.size()) : 1.0F);
    program.set("uAutoExposure", frame.autoExposure && !debug);

    bindTexture(radianceUnit, m_radiance.texture.get());
    bindTexture(bloomUnit, bloomEnabled ? m_bloomLevels.front().texture.get() : m_radiance.texture.get());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, exposureBinding, m_exposure.get());

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, frame.outputSize.x, frame.outputSize.y);
    program.use();
    glBindVertexArray(m_emptyVao.get());
    glDrawArrays(GL_TRIANGLES, 0, 3); // one triangle covering the screen, generated in the vertex shader
}

namespace {

void endSection(gl::GpuTimer* const timer, const GpuPass pass)
{
    if (timer != nullptr) {
        timer->endSection(index(pass));
    }
}

} // namespace

void BlackHoleRenderer::draw(const FrameParams& frame, gl::GpuTimer& timer)
{
    prepare(frame);
    refine(frame, &timer);
    finish(frame, 0, &timer);
}

void BlackHoleRenderer::prepare(const FrameParams& frame)
{
    const glm::ivec2 traceSize {
        glm::max(glm::ivec2 {1}, glm::ivec2 {glm::round(glm::vec2 {frame.outputSize} * frame.renderScale)})};
    if (traceSize != m_radiance.size) {
        resizeTargets(traceSize);
    }
    const FrameUniforms uniforms {frameUniforms(frame)};
    if (!frame.accumulate || m_traced != uniforms) {
        m_samples = 0; // anything the trace depends on changed: start over
    }
    m_traced = uniforms;
    glBindBuffer(GL_UNIFORM_BUFFER, m_frameUniforms.get());
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(uniforms), &uniforms);
}

void BlackHoleRenderer::refine(const FrameParams& frame, gl::GpuTimer* const timer)
{
    if (m_samples < maxAccumulatedSamples) {
        trace(frame.accumulate ? jitter(m_samples) : glm::vec2 {0.0F}, frame.accumulate);
        m_samples = frame.accumulate ? m_samples + 1 : 0;
    }
    endSection(timer, GpuPass::TRACE);
}

void BlackHoleRenderer::finish(const FrameParams& frame, const GLuint framebuffer, gl::GpuTimer* const timer)
{
    const bool debug {frame.debugView != DebugView::NONE};
    if (frame.autoExposure && !debug) {
        meterExposure(frame);
    } else {
        m_exposureMetered = false; // turned on again later: jump straight to the target
    }
    endSection(timer, GpuPass::EXPOSURE);
    if (frame.bloomEnabled && !debug && !m_bloomLevels.empty()) {
        bloom();
    }
    endSection(timer, GpuPass::BLOOM);
    present(frame, framebuffer);
    endSection(timer, GpuPass::PRESENT);
}

auto BlackHoleRenderer::renderImage(const FrameParams& frame, const std::uint32_t samples)
    -> std::expected<RgbaImage, std::string>
{
    const glm::ivec2 size {frame.outputSize};
    GLint maxSize {};
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    if (size.x <= 0 || size.y <= 0 || std::max(size.x, size.y) > maxSize) {
        return std::unexpected {std::format("{}x{} is not a size this GPU can render (limit {})", size.x, size.y, maxSize)};
    }

    // present.frag writes sRGB-encoded values itself (as for the window), so a plain RGBA8 target.
    gl::Texture color {gl::createTexture()};
    glBindTexture(GL_TEXTURE_2D, color.get());
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, size.x, size.y);
    const gl::Framebuffer framebuffer {gl::createFramebuffer()};
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color.get(), 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return std::unexpected {std::string {"offscreen framebuffer incomplete"}};
    }

    FrameParams still {frame};
    still.accumulate = true;
    still.frameSeconds = 0.0F; // the exposure stays where the interactive view has it
    prepare(still);
    m_samples = 0;
    for (std::uint32_t sample {0}; sample < std::max(samples, 1U); ++sample) {
        refine(still, nullptr);
    }
    finish(still, framebuffer.get(), nullptr);

    RgbaImage image {.width = size.x, .height = size.y, .pixels = {}};
    const std::size_t rowBytes {4 * static_cast<std::size_t>(size.x)};
    std::vector<std::uint8_t> bottomUp(rowBytes * static_cast<std::size_t>(size.y));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, bottomUp.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (glGetError() == GL_OUT_OF_MEMORY) {
        return std::unexpected {std::format("{}x{}: out of GPU memory", size.x, size.y)};
    }
    // OpenGL's rows run bottom to top; image files run top to bottom.
    image.pixels.reserve(bottomUp.size());
    for (std::size_t row {static_cast<std::size_t>(size.y)}; row > 0; --row) {
        const auto begin {bottomUp.begin() + static_cast<std::ptrdiff_t>((row - 1) * rowBytes)};
        image.pixels.insert(image.pixels.end(), begin, begin + static_cast<std::ptrdiff_t>(rowBytes));
    }
    return image;
}

} // namespace bh
