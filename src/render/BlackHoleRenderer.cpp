#include "render/BlackHoleRenderer.hpp"

#include "render/FrameUniforms.hpp"
#include "render/GpuPass.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>

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
constexpr GLuint bloomUnit = 1;

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
{
    glBindBuffer(GL_UNIFORM_BUFFER, m_frameUniforms.get());
    glBufferData(GL_UNIFORM_BUFFER, sizeof(FrameUniforms), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, frameUniformBinding, m_frameUniforms.get());
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
    for (const auto* const program : {&trace, &downsample, &upsample, &present}) {
        if (!*program) {
            return std::unexpected {program->error()};
        }
    }
    return Programs {
        .trace = std::move(*trace),
        .bloomDownsample = std::move(*downsample),
        .bloomUpsample = std::move(*upsample),
        .present = std::move(*present),
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
    return {};
}

bool BlackHoleRenderer::shaderSourcesChanged() const
{
    return newestModification(m_shaderDir) > m_shaderSnapshot;
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

    m_radiance = createTarget(traceSize);
    m_bloomLevels.clear();
    for (glm::ivec2 size {traceSize / 2};
         m_bloomLevels.size() < maxBloomLevels && std::min(size.x, size.y) >= minBloomLevelSize; size /= 2) {
        m_bloomLevels.push_back(createTarget(size));
    }
}

void BlackHoleRenderer::uploadFrameUniforms(const FrameParams& frame)
{
    const FrameUniforms uniforms {
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
        .flags = (frame.diskEnabled ? frameFlagDisk : 0U) | (frame.starsEnabled ? frameFlagStars : 0U),
        .debugView = static_cast<std::uint32_t>(std::to_underlying(frame.debugView)),
    };
    glBindBuffer(GL_UNIFORM_BUFFER, m_frameUniforms.get());
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(uniforms), &uniforms);
}

void BlackHoleRenderer::trace()
{
    m_programs.trace.use();
    glBindImageTexture(0, m_radiance.texture.get(), 0, GL_FALSE, 0, GL_WRITE_ONLY, hdrFormat);
    dispatchOver(m_radiance.size);
    imageWriteBarrier();
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

void BlackHoleRenderer::present(const FrameParams& frame)
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

    bindTexture(radianceUnit, m_radiance.texture.get());
    bindTexture(bloomUnit, bloomEnabled ? m_bloomLevels.front().texture.get() : m_radiance.texture.get());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, frame.outputSize.x, frame.outputSize.y);
    program.use();
    glBindVertexArray(m_emptyVao.get());
    glDrawArrays(GL_TRIANGLES, 0, 3); // one triangle covering the screen, generated in the vertex shader
}

void BlackHoleRenderer::draw(const FrameParams& frame, gl::GpuTimer& timer)
{
    const glm::ivec2 traceSize {
        glm::max(glm::ivec2 {1}, glm::ivec2 {glm::round(glm::vec2 {frame.outputSize} * frame.renderScale)})};
    if (traceSize != m_radiance.size) {
        resizeTargets(traceSize);
    }
    uploadFrameUniforms(frame);

    trace();
    timer.endSection(index(GpuPass::TRACE));
    if (frame.bloomEnabled && frame.debugView == DebugView::NONE && !m_bloomLevels.empty()) {
        bloom();
    }
    timer.endSection(index(GpuPass::BLOOM));
    present(frame);
    timer.endSection(index(GpuPass::PRESENT));
}

} // namespace bh
