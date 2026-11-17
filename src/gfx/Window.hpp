#pragma once

#include <glm/glm.hpp>

#include <memory>
#include <string>

struct GLFWwindow;

namespace bh {

// Owns glfwInit()/glfwTerminate(); must outlive every Window.
class GlfwLibrary final {
public:
    GlfwLibrary();
    ~GlfwLibrary();

    GlfwLibrary(const GlfwLibrary&) = delete;
    GlfwLibrary& operator=(const GlfwLibrary&) = delete;
    GlfwLibrary(GlfwLibrary&&) = delete;
    GlfwLibrary& operator=(GlfwLibrary&&) = delete;
};

struct WindowDesc {
    int width {1600};
    int height {900};
    std::string title {"BlackHole"};
    bool vsync {true};
    bool debugContext {false};
};

// A GLFW window with a current OpenGL 4.3 core context and GLAD loaded.
// Not movable: GLFW callbacks hold a pointer to it.
class Window final {
public:
    explicit Window(const WindowDesc& desc);
    ~Window() = default;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] GLFWwindow* handle() const { return m_window.get(); }
    [[nodiscard]] bool shouldClose() const;
    void requestClose();
    void swapBuffers();
    void setVsync(bool enabled);
    // Fullscreen on the monitor that holds most of the window, at that monitor's current video mode (no
    // mode switch); switching back restores the previous windowed position and size.
    void setFullscreen(bool enabled);
    [[nodiscard]] bool isFullscreen() const;

    [[nodiscard]] glm::ivec2 framebufferSize() const;
    // The monitor's DPI scale as the platform reports it (1 on a standard screen, 2 on a "Retina" one).
    [[nodiscard]] float contentScale() const;
    [[nodiscard]] glm::dvec2 cursorPosition() const;
    [[nodiscard]] bool isMouseButtonDown(int button) const;
    [[nodiscard]] bool isKeyDown(int key) const;

    // Scroll accumulated by the GLFW callback since the last call.
    [[nodiscard]] double takeScroll();

private:
    struct Destroyer {
        void operator()(GLFWwindow* window) const noexcept;
    };

    std::unique_ptr<GLFWwindow, Destroyer> m_window;
    double m_scroll {};
    bool m_vsync {true};              // re-applied after a fullscreen switch, which may reset it
    glm::ivec2 m_windowedPosition {}; // to restore when leaving fullscreen
    glm::ivec2 m_windowedSize {};
};

} // namespace bh
