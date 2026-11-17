#include "gfx/Window.hpp"

#include "gfx/GlDebug.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <format>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace bh {

namespace {

constexpr int requiredGlMajor = 4;
constexpr int requiredGlMinor = 3;

void onGlfwError(const int code, const char* const description)
{
    std::cerr << "[GLFW " << code << "] " << description << '\n';
}

} // namespace

GlfwLibrary::GlfwLibrary()
{
    glfwSetErrorCallback(onGlfwError);
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error {"glfwInit failed"};
    }
}

GlfwLibrary::~GlfwLibrary()
{
    glfwTerminate();
}

void Window::Destroyer::operator()(GLFWwindow* const window) const noexcept
{
    glfwDestroyWindow(window);
}

Window::Window(const WindowDesc& desc)
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, requiredGlMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, requiredGlMinor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, desc.debugContext ? GLFW_TRUE : GLFW_FALSE);

    m_window.reset(glfwCreateWindow(desc.width, desc.height, desc.title.c_str(), nullptr, nullptr));
    if (!m_window) {
        throw std::runtime_error {
            std::format("cannot create a window with an OpenGL {}.{} core context", requiredGlMajor, requiredGlMinor)};
    }
    glfwMakeContextCurrent(m_window.get());

    const int version {gladLoadGL(glfwGetProcAddress)};
    if (GLAD_VERSION_MAJOR(version) < requiredGlMajor
        || (GLAD_VERSION_MAJOR(version) == requiredGlMajor && GLAD_VERSION_MINOR(version) < requiredGlMinor)) {
        throw std::runtime_error {"failed to load OpenGL 4.3 functions"};
    }
    if (desc.debugContext) {
        gl::enableDebugOutput();
    }
    setVsync(desc.vsync);

    glfwSetWindowUserPointer(m_window.get(), this);
    // Installed before ImGui's GLFW backend, which chains to it.
    glfwSetScrollCallback(m_window.get(), [](GLFWwindow* const window, double, const double yOffset) {
        static_cast<Window*>(glfwGetWindowUserPointer(window))->m_scroll += yOffset;
    });
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(m_window.get()) == GLFW_TRUE;
}

void Window::requestClose()
{
    glfwSetWindowShouldClose(m_window.get(), GLFW_TRUE);
}

void Window::swapBuffers()
{
    glfwSwapBuffers(m_window.get());
}

void Window::setVsync(const bool enabled)
{
    glfwSwapInterval(enabled ? 1 : 0);
}

glm::ivec2 Window::framebufferSize() const
{
    glm::ivec2 size {};
    glfwGetFramebufferSize(m_window.get(), &size.x, &size.y);
    return size;
}

glm::dvec2 Window::cursorPosition() const
{
    glm::dvec2 position {};
    glfwGetCursorPos(m_window.get(), &position.x, &position.y);
    return position;
}

bool Window::isMouseButtonDown(const int button) const
{
    return glfwGetMouseButton(m_window.get(), button) == GLFW_PRESS;
}

bool Window::isKeyDown(const int key) const
{
    return glfwGetKey(m_window.get(), key) == GLFW_PRESS;
}

double Window::takeScroll()
{
    return std::exchange(m_scroll, 0.0);
}

} // namespace bh
