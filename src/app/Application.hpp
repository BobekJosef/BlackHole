#pragma once

#include "gfx/GpuTimer.hpp"
#include "gfx/Window.hpp"
#include "render/BlackHoleRenderer.hpp"
#include "scene/OrbitCamera.hpp"
#include "scene/SceneSettings.hpp"
#include "ui/ImGuiLayer.hpp"

#include <glm/glm.hpp>

#include <string>

namespace bh {

class Application final {
public:
    Application();

    void run();

private:
    void handleInput();
    [[nodiscard]] FrameParams frameParams(glm::ivec2 framebufferSize, float time) const;
    void reloadShaders();
    void pollShaderFiles(double now);

    // Declaration order is construction order: GLFW, then the window and its GL context,
    // then everything that makes GL calls. Destruction runs in reverse.
    GlfwLibrary m_glfw {};
    Window m_window;
    ImGuiLayer m_gui;
    BlackHoleRenderer m_renderer;
    gl::GpuTimer m_gpuTimer;

    OrbitCamera m_camera {};
    SceneSettings m_settings {};
    std::string m_glRenderer {};
    std::string m_shaderError {};
    glm::dvec2 m_lastCursor {};
    bool m_vsync {true};
    bool m_reloadKeyWasDown {};
    double m_smoothedFrameSeconds {1.0 / 60.0};
    double m_nextShaderPoll {};
};

} // namespace bh
