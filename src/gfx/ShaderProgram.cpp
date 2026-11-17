#include "gfx/ShaderProgram.hpp"

#include "gfx/ShaderPreprocessor.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <utility>
#include <vector>

namespace bh::gl {

namespace {

[[nodiscard]] GLenum glStage(const ShaderStage stage)
{
    switch (stage) {
    case ShaderStage::VERTEX: return GL_VERTEX_SHADER;
    case ShaderStage::FRAGMENT: return GL_FRAGMENT_SHADER;
    case ShaderStage::COMPUTE: return GL_COMPUTE_SHADER;
    }
    std::unreachable();
}

[[nodiscard]] std::string shaderLog(const GLuint shader)
{
    GLint length {};
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(length), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    return log;
}

[[nodiscard]] std::string programLog(const GLuint program)
{
    GLint length {};
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(length), '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    return log;
}

[[nodiscard]] std::expected<Shader, std::string> compile(const ShaderFile& file)
{
    const auto source {preprocessShader(file.path)};
    if (!source) {
        return std::unexpected {source.error()};
    }
    Shader shader {glCreateShader(glStage(file.stage))};
    const GLchar* const text {source->source.c_str()};
    glShaderSource(shader.get(), 1, &text, nullptr);
    glCompileShader(shader.get());

    GLint status {};
    glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        return std::unexpected {annotateShaderLog(shaderLog(shader.get()), source->files)};
    }
    return shader;
}

} // namespace

std::expected<ShaderProgram, std::string> ShaderProgram::fromFiles(const std::span<const ShaderFile> files)
{
    std::vector<Shader> shaders {};
    shaders.reserve(files.size());
    for (const ShaderFile& file : files) {
        auto shader {compile(file)};
        if (!shader) {
            return std::unexpected {std::move(shader.error())};
        }
        shaders.push_back(std::move(*shader));
    }

    Program program {glCreateProgram()};
    for (const Shader& shader : shaders) {
        glAttachShader(program.get(), shader.get());
    }
    glLinkProgram(program.get());
    for (const Shader& shader : shaders) {
        glDetachShader(program.get(), shader.get());
    }

    GLint status {};
    glGetProgramiv(program.get(), GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        return std::unexpected {"link error:\n" + programLog(program.get())};
    }
    return ShaderProgram {std::move(program)};
}

ShaderProgram::ShaderProgram(Program program)
    : m_program {std::move(program)}
{
}

void ShaderProgram::use() const
{
    glUseProgram(m_program.get());
}

GLint ShaderProgram::location(const std::string_view name)
{
    if (const auto it {m_locations.find(name)}; it != m_locations.end()) {
        return it->second;
    }
    std::string key {name};
    const GLint loc {glGetUniformLocation(m_program.get(), key.c_str())};
    m_locations.emplace(std::move(key), loc);
    return loc;
}

void ShaderProgram::set(const std::string_view name, const bool value)
{
    glProgramUniform1i(m_program.get(), location(name), value ? 1 : 0);
}

void ShaderProgram::set(const std::string_view name, const int value)
{
    glProgramUniform1i(m_program.get(), location(name), value);
}

void ShaderProgram::set(const std::string_view name, const std::uint32_t value)
{
    glProgramUniform1ui(m_program.get(), location(name), value);
}

void ShaderProgram::set(const std::string_view name, const float value)
{
    glProgramUniform1f(m_program.get(), location(name), value);
}

void ShaderProgram::set(const std::string_view name, const glm::vec2& value)
{
    glProgramUniform2fv(m_program.get(), location(name), 1, glm::value_ptr(value));
}

void ShaderProgram::set(const std::string_view name, const glm::vec3& value)
{
    glProgramUniform3fv(m_program.get(), location(name), 1, glm::value_ptr(value));
}

void ShaderProgram::set(const std::string_view name, const glm::mat3& value)
{
    glProgramUniformMatrix3fv(m_program.get(), location(name), 1, GL_FALSE, glm::value_ptr(value));
}

} // namespace bh::gl
