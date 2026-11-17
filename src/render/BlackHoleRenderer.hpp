#pragma once

#include "gfx/GlObject.hpp"
#include "gfx/GpuTimer.hpp"
#include "gfx/ShaderProgram.hpp"
#include "render/FrameParams.hpp"

#include <glm/glm.hpp>

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace bh {

// Renders a frame in three GPU passes:
//   1. trace   (compute) one ray per pixel at renderScale x window size -> HDR radiance (r11f_g11f_b10f)
//   2. bloom   (compute) down-sampling chain, then up-sampling back to half the trace size
//   3. present (fragment) up-scale, bloom mix, exposure, tone mapping, sRGB -> default framebuffer
class BlackHoleRenderer final {
public:
    // Throws if the initial shaders do not compile.
    explicit BlackHoleRenderer(std::filesystem::path shaderDir);

    // Recompiles all shaders from disk; on any failure every previous program stays active.
    [[nodiscard]] std::expected<void, std::string> reloadShaders();
    // True if a file under the shader directory changed since the last (re)load attempt.
    [[nodiscard]] bool shaderSourcesChanged() const;

    void draw(const FrameParams& frame, gl::GpuTimer& timer);

    // Disk temperature profile T / T_peak over u = sqrt((r - r_in) / (r_out - r_in)), u in [0, 1]
    // (kerr::diskTemperatureProfile). Any number of samples; re-upload whenever spin, orbit or r_out change.
    void setDiskProfile(std::span<const float> profile);

    [[nodiscard]] glm::ivec2 traceResolution() const { return m_radiance.size; }

private:
    struct Programs {
        gl::ShaderProgram trace;
        gl::ShaderProgram bloomDownsample;
        gl::ShaderProgram bloomUpsample;
        gl::ShaderProgram present;
    };

    struct RenderTarget {
        gl::Texture texture {};
        glm::ivec2 size {};
    };

    [[nodiscard]] std::expected<Programs, std::string> loadPrograms() const;
    void resizeTargets(glm::ivec2 traceSize);
    void uploadFrameUniforms(const FrameParams& frame);
    void trace();
    void bloom();
    void present(const FrameParams& frame);

    std::filesystem::path m_shaderDir;
    std::filesystem::file_time_type m_shaderSnapshot {};
    Programs m_programs;
    gl::Buffer m_frameUniforms {gl::createBuffer()};
    gl::VertexArray m_emptyVao {gl::createVertexArray()};
    gl::Texture m_blackbodyLut;            // linear sRGB radiance over log10 T, from bh::blackbodyRgb
    gl::Texture m_diskProfile {};
    int m_diskProfileSize {};
    RenderTarget m_radiance {};
    std::vector<RenderTarget> m_bloomLevels {}; // [0] is half the trace size, each next one half again
};

} // namespace bh
