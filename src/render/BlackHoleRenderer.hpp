#pragma once

#include "gfx/GlObject.hpp"
#include "gfx/ShaderProgram.hpp"
#include "render/FrameParams.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace bh {

// Draws the black hole scene as one full-screen pass.
class BlackHoleRenderer final {
public:
    // Throws if the initial shaders do not compile.
    explicit BlackHoleRenderer(std::filesystem::path shaderDir);

    // Recompiles the shaders from disk; on failure the previous program stays active.
    [[nodiscard]] std::expected<void, std::string> reloadShaders();

    void draw(const FrameParams& frame);

private:
    [[nodiscard]] std::expected<gl::ShaderProgram, std::string> loadProgram() const;

    std::filesystem::path m_shaderDir;
    gl::ShaderProgram m_program;
    gl::VertexArray m_emptyVao {gl::createVertexArray()};
};

} // namespace bh
