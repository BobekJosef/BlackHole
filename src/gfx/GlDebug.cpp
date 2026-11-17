#include "gfx/GlDebug.hpp"

#include <glad/gl.h>

#include <iostream>
#include <string_view>

namespace bh::gl {

namespace {

[[nodiscard]] std::string_view severityName(const GLenum severity)
{
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH: return "high";
    case GL_DEBUG_SEVERITY_MEDIUM: return "medium";
    case GL_DEBUG_SEVERITY_LOW: return "low";
    default: return "info";
    }
}

void GLAPIENTRY onDebugMessage([[maybe_unused]] const GLenum source, [[maybe_unused]] const GLenum type,
                               const GLuint id, const GLenum severity, const GLsizei length,
                               const GLchar* const message, [[maybe_unused]] const void* const userParam)
{
    const std::string_view text {message, static_cast<std::size_t>(length)};
    std::cerr << "[GL " << severityName(severity) << " #" << id << "] " << text << '\n';
}

} // namespace

void enableDebugOutput()
{
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(onDebugMessage, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
}

} // namespace bh::gl
