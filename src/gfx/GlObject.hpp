#pragma once
// Move-only RAII owner of one OpenGL object name.

#include <glad/gl.h>

#include <utility>

namespace bh::gl {

template <typename Deleter>
class Object final {
public:
    Object() = default;
    explicit Object(const GLuint name) : m_name {name} {}

    ~Object() { reset(); }

    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;
    Object(Object&& other) noexcept : m_name {std::exchange(other.m_name, 0U)} {}
    Object& operator=(Object&& other) noexcept
    {
        if (this != &other) {
            reset();
            m_name = std::exchange(other.m_name, 0U);
        }
        return *this;
    }

    [[nodiscard]] GLuint get() const { return m_name; }

private:
    void reset() noexcept
    {
        // 0 is OpenGL's own "no object" name, which every glDelete* ignores anyway.
        if (m_name != 0U) {
            Deleter {}(m_name);
            m_name = 0U;
        }
    }

    GLuint m_name {};
};

namespace detail {

struct ProgramDeleter {
    void operator()(const GLuint name) const noexcept { glDeleteProgram(name); }
};
struct ShaderDeleter {
    void operator()(const GLuint name) const noexcept { glDeleteShader(name); }
};
struct VertexArrayDeleter {
    void operator()(const GLuint name) const noexcept { glDeleteVertexArrays(1, &name); }
};
struct TextureDeleter {
    void operator()(const GLuint name) const noexcept { glDeleteTextures(1, &name); }
};
struct BufferDeleter {
    void operator()(const GLuint name) const noexcept { glDeleteBuffers(1, &name); }
};
struct QueryDeleter {
    void operator()(const GLuint name) const noexcept { glDeleteQueries(1, &name); }
};
struct FramebufferDeleter {
    void operator()(const GLuint name) const noexcept { glDeleteFramebuffers(1, &name); }
};

} // namespace detail

using Program = Object<detail::ProgramDeleter>;
using Shader = Object<detail::ShaderDeleter>;
using VertexArray = Object<detail::VertexArrayDeleter>;
using Texture = Object<detail::TextureDeleter>;
using Buffer = Object<detail::BufferDeleter>;
using Query = Object<detail::QueryDeleter>;
using Framebuffer = Object<detail::FramebufferDeleter>;

[[nodiscard]] inline VertexArray createVertexArray()
{
    GLuint name {};
    glGenVertexArrays(1, &name); // not glCreateVertexArrays: that is OpenGL 4.5, the minimum here is 4.3
    return VertexArray {name};
}

[[nodiscard]] inline Buffer createBuffer()
{
    GLuint name {};
    glGenBuffers(1, &name);
    return Buffer {name};
}

[[nodiscard]] inline Texture createTexture()
{
    GLuint name {};
    glGenTextures(1, &name);
    return Texture {name};
}

[[nodiscard]] inline Framebuffer createFramebuffer()
{
    GLuint name {};
    glGenFramebuffers(1, &name);
    return Framebuffer {name};
}

[[nodiscard]] inline Query createQuery()
{
    GLuint name {};
    glGenQueries(1, &name);
    return Query {name};
}

} // namespace bh::gl
