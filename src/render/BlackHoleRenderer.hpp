#pragma once

#include "assets/PngFile.hpp"
#include "assets/SkyImage.hpp"
#include "gfx/GlObject.hpp"
#include "gfx/GpuTimer.hpp"
#include "gfx/ShaderProgram.hpp"
#include "render/FrameParams.hpp"
#include "render/FrameUniforms.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace bh {

// Renders a frame in three GPU passes:
//   1. trace    (compute) one ray per pixel at renderScale x window size, jittered within the pixel, folded
//               into a running mean (rgba32f) while the trace inputs stay the same -> HDR radiance
//               (r11f_g11f_b10f). Once maxAccumulatedSamples frames are averaged it is skipped.
//   2. exposure (compute) with auto exposure: mipmaps of the radiance and one metering work group
//   3. bloom    (compute) down-sampling chain, then up-sampling back to half the trace size
//   4. present  (fragment) up-scale, bloom mix, exposure, tone mapping, sRGB -> default framebuffer
class BlackHoleRenderer final {
public:
    // Throws if the initial shaders do not compile.
    explicit BlackHoleRenderer(std::filesystem::path shaderDir);

    // Recompiles all shaders from disk; on any failure every previous program stays active.
    [[nodiscard]] std::expected<void, std::string> reloadShaders();
    // True if a file under the shader directory changed since the last (re)load attempt.
    [[nodiscard]] bool shaderSourcesChanged() const;

    void draw(const FrameParams& frame, gl::GpuTimer& timer);

    // Renders `frame` (at frame.outputSize and frame.renderScale) into an offscreen image instead of the
    // window, and reads it back, top row first: a screenshot without the UI, at any size the GPU allows.
    // Stalls until the GPU is done. The next draw() resizes the internal targets back.
    // `samples` jittered frames are averaged first (anti-aliasing).
    [[nodiscard]] std::expected<RgbaImage, std::string> renderImage(const FrameParams& frame, std::uint32_t samples);

    static constexpr std::uint32_t maxAccumulatedSamples {256};
    // Frames averaged into the image shown (1 while anything changes).
    [[nodiscard]] std::uint32_t accumulatedSamples() const { return m_samples; }

    // Disk temperature profile T / T_peak over u = sqrt((r - r_in) / (r_out - r_in)), u in [0, 1]
    // (kerr::diskTemperatureProfile). Any number of samples; re-upload whenever spin, orbit or r_out change.
    void setDiskProfile(std::span<const float> profile);

    // Uploads a decoded sky map with a full mipmap chain, replacing the previous one. Fails, keeping the
    // previous map, if it is larger than the GPU allows.
    [[nodiscard]] std::expected<void, std::string> setSkyImage(const SkyImage& image);
    // Projection of the sky map held, if any: what FrameParams::sky may name.
    [[nodiscard]] std::optional<SkyProjection> skyProjection() const { return m_skyProjection; }

    [[nodiscard]] glm::ivec2 traceResolution() const { return m_radiance.size; }

private:
    struct Programs {
        gl::ShaderProgram trace;
        gl::ShaderProgram bloomDownsample;
        gl::ShaderProgram bloomUpsample;
        gl::ShaderProgram present;
        gl::ShaderProgram exposure;
    };

    struct RenderTarget {
        gl::Texture texture {};
        glm::ivec2 size {};
    };

    [[nodiscard]] std::expected<Programs, std::string> loadPrograms() const;
    void resizeTargets(glm::ivec2 traceSize);
    [[nodiscard]] FrameUniforms frameUniforms(const FrameParams& frame) const;
    // Resizes the targets, uploads the uniforms, and restarts the accumulation if they changed.
    void prepare(const FrameParams& frame);
    // One more traced (and accumulated) frame, unless converged.
    void refine(const FrameParams& frame, gl::GpuTimer* timer);
    void meterExposure(const FrameParams& frame);
    // Exposure, bloom and present into `framebuffer`.
    void finish(const FrameParams& frame, GLuint framebuffer, gl::GpuTimer* timer);
    // The requested sky, or NONE if it names a map projection the renderer does not hold.
    [[nodiscard]] Sky availableSky(Sky requested) const;
    void trace(glm::vec2 jitter, bool accumulate);
    void bloom();
    void present(const FrameParams& frame, GLuint framebuffer);

    std::filesystem::path m_shaderDir;
    std::filesystem::file_time_type m_shaderSnapshot {};
    Programs m_programs;
    gl::Buffer m_frameUniforms {gl::createBuffer()};
    gl::VertexArray m_emptyVao {gl::createVertexArray()};
    gl::Texture m_blackbodyLut;            // linear sRGB radiance over log10 T, from bh::blackbodyRgb
    gl::Texture m_diskProfile {};
    int m_diskProfileSize {};
    gl::Texture m_skyTexture {};               // GL_TEXTURE_2D (equirectangular) or GL_TEXTURE_CUBE_MAP
    std::optional<SkyProjection> m_skyProjection {};
    float m_maxAnisotropy {1.0F};              // 1 without anisotropic filtering support
    RenderTarget m_radiance {};    // what is shown: the running mean, with mipmaps (for the metering)
    RenderTarget m_accumulated {}; // the running mean at full precision (rgba32f)
    gl::Buffer m_exposure {gl::createBuffer()}; // SSBO: log2 auto exposure, metered luminance
    std::optional<FrameUniforms> m_traced {};   // the uniforms m_accumulated was traced with
    std::uint32_t m_samples {};
    bool m_exposureMetered {};                  // false: the next metering jumps to its target
    std::vector<RenderTarget> m_bloomLevels {}; // [0] is half the trace size, each next one half again
};

} // namespace bh
