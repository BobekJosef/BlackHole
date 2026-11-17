#include "gfx/Window.hpp"

#include "gfx/GlDebug.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <format>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace bh {

namespace {

constexpr int requiredGlMajor = 4;
constexpr int requiredGlMinor = 3;

// The monitor with the largest overlap with the window rectangle (GLFW has no call for it), else the primary.
[[nodiscard]] GLFWmonitor* monitorOf(GLFWwindow* const window)
{
    glm::ivec2 position {};
    glm::ivec2 size {};
    glfwGetWindowPos(window, &position.x, &position.y);
    glfwGetWindowSize(window, &size.x, &size.y);

    int count {};
    GLFWmonitor** const monitors {glfwGetMonitors(&count)};
    GLFWmonitor* best {glfwGetPrimaryMonitor()};
    int bestOverlap {0};
    for (int i {0}; i < count; ++i) {
        GLFWmonitor* const monitor {monitors[i]};
        const GLFWvidmode* const mode {glfwGetVideoMode(monitor)};
        glm::ivec2 origin {};
        glfwGetMonitorPos(monitor, &origin.x, &origin.y);
        const glm::ivec2 low {glm::max(position, origin)};
        const glm::ivec2 high {glm::min(position + size, origin + glm::ivec2 {mode->width, mode->height})};
        const glm::ivec2 extent {glm::max(high - low, glm::ivec2 {0})};
        if (extent.x * extent.y > bestOverlap) {
            bestOverlap = extent.x * extent.y;
            best = monitor;
        }
    }
    return best;
}

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
    glfwWindowHint(GLFW_VISIBLE, desc.visible ? GLFW_TRUE : GLFW_FALSE);

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
    m_vsync = enabled;
    glfwSwapInterval(enabled ? 1 : 0);
}

void Window::setFullscreen(const bool enabled)
{
    if (enabled == isFullscreen()) {
        return;
    }
    GLFWwindow* const window {m_window.get()};
    if (enabled) {
        glfwGetWindowPos(window, &m_windowedPosition.x, &m_windowedPosition.y);
        glfwGetWindowSize(window, &m_windowedSize.x, &m_windowedSize.y);
        GLFWmonitor* const monitor {monitorOf(window)};
        const GLFWvidmode* const mode {glfwGetVideoMode(monitor)};
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        glfwSetWindowMonitor(window, nullptr, m_windowedPosition.x, m_windowedPosition.y, m_windowedSize.x,
                             m_windowedSize.y, GLFW_DONT_CARE);
    }
    setVsync(m_vsync);
}

bool Window::isFullscreen() const
{
    return glfwGetWindowMonitor(m_window.get()) != nullptr;
}

glm::ivec2 Window::framebufferSize() const
{
    glm::ivec2 size {};
    glfwGetFramebufferSize(m_window.get(), &size.x, &size.y);
    return size;
}

float Window::contentScale() const
{
    glm::vec2 scale {1.0F};
    glfwGetWindowContentScale(m_window.get(), &scale.x, &scale.y);
    return std::max(scale.x, scale.y);
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

void Window::setCursorCaptured(const bool captured)
{
    GLFWwindow* const window {m_window.get()};
    glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
    }
}

bool Window::isCursorCaptured() const
{
    return glfwGetInputMode(m_window.get(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

double Window::takeScroll()
{
    return std::exchange(m_scroll, 0.0);
}

} // namespace bh
