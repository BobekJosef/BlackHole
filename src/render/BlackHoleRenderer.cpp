#include "render/BlackHoleRenderer.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace bh {

BlackHoleRenderer::BlackHoleRenderer(std::filesystem::path shaderDir)
    : m_shaderDir {std::move(shaderDir)}
    , m_program {[this] {
        auto program {loadProgram()};
        if (!program) {
            throw std::runtime_error {"shader compilation failed: " + program.error()};
        }
        return std::move(*program);
    }()}
{
}

std::expected<gl::ShaderProgram, std::string> BlackHoleRenderer::loadProgram() const
{
    const std::array files {
        gl::ShaderFile {gl::ShaderStage::VERTEX, m_shaderDir / "fullscreen.vert"},
        gl::ShaderFile {gl::ShaderStage::FRAGMENT, m_shaderDir / "blackhole.frag"},
    };
    return gl::ShaderProgram::fromFiles(files);
}

std::expected<void, std::string> BlackHoleRenderer::reloadShaders()
{
    auto program {loadProgram()};
    if (!program) {
        return std::unexpected {std::move(program.error())};
    }
    m_program = std::move(*program);
    return {};
}

void BlackHoleRenderer::draw(const FrameParams& frame)
{
    m_program.set("uResolution", glm::vec2 {frame.resolution});
    m_program.set("uCameraPosition", frame.cameraPosition);
    m_program.set("uCameraBasis", frame.cameraBasis);
    m_program.set("uTanHalfFov", frame.tanHalfFov);
    m_program.set("uShadowRadius", frame.shadowRadius);
    m_program.set("uDiskEnabled", frame.diskEnabled);
    m_program.set("uDiskInnerRadius", frame.diskInnerRadius);
    m_program.set("uDiskOuterRadius", frame.diskOuterRadius);
    m_program.set("uStarsEnabled", frame.starsEnabled);
    m_program.set("uExposure", frame.exposure);
    m_program.set("uTime", frame.time);

    glViewport(0, 0, frame.resolution.x, frame.resolution.y);
    m_program.use();
    glBindVertexArray(m_emptyVao.get());
    glDrawArrays(GL_TRIANGLES, 0, 3); // one triangle covering the screen, generated in the vertex shader
}

} // namespace bh
