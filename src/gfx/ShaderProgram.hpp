#pragma once

#include "gfx/GlObject.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>

namespace bh::gl {

enum class ShaderStage { VERTEX, FRAGMENT, COMPUTE };

struct ShaderFile {
    ShaderStage stage {ShaderStage::VERTEX};
    std::filesystem::path path {};
};

// A linked GLSL program. Uniforms are set with glProgramUniform* (no bind needed),
// and their locations are cached by name.
class ShaderProgram final {
public:
    // Compiles and links the given stages; the error carries the file name and driver log.
    [[nodiscard]] static std::expected<ShaderProgram, std::string> fromFiles(std::span<const ShaderFile> files);

    void use() const;

    void set(std::string_view name, bool value);
    void set(std::string_view name, int value);
    void set(std::string_view name, std::uint32_t value); // GLSL uint
    void set(std::string_view name, float value);
    void set(std::string_view name, const glm::vec2& value);
    void set(std::string_view name, const glm::vec3& value);
    void set(std::string_view name, const glm::mat3& value);

private:
    explicit ShaderProgram(Program program);

    // -1 (OpenGL's value) for a uniform the compiler optimised out; glProgramUniform* ignores it.
    [[nodiscard]] GLint location(std::string_view name);

    Program m_program;
    std::map<std::string, GLint, std::less<>> m_locations;
};

} // namespace bh::gl
