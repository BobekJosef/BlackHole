#include "app/Application.hpp"

#include "blackhole/config.hpp"
#include "physics/Kerr.hpp"
#include "render/GpuPass.hpp"
#include "ui/ControlPanel.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <filesystem>
#include <format>

namespace bh {

namespace {

// Weight of the newest frame in the exponential moving average of the frame time.
constexpr double frameTimeSmoothing = 0.05;
constexpr double shaderPollSeconds = 1.0;

[[nodiscard]] std::string glString(const GLenum name)
{
    // glGetString returns const GLubyte*; the bytes are a NUL-terminated character string.
    return reinterpret_cast<const char*>(glGetString(name));
}

} // namespace

Application::Application()
    : m_window {WindowDesc {.title = std::format("BlackHole {}", config::projectVersion),
                            .vsync = true,
                            .debugContext = config::debugBuild}}
    , m_gui {m_window.handle()}
    , m_renderer {std::filesystem::path {config::shaderDir}}
    , m_gpuTimer {gpuPassNames.size()}
    , m_glRenderer {glString(GL_RENDERER) + " | OpenGL " + glString(GL_VERSION)}
    , m_lastCursor {m_window.cursorPosition()}
{
}

void Application::run()
{
    double lastTime {glfwGetTime()};
    while (!m_window.shouldClose()) {
        glfwPollEvents();

        const glm::ivec2 framebuffer {m_window.framebufferSize()};
        if (framebuffer.x <= 0 || framebuffer.y <= 0) { // minimised
            glfwWaitEvents();
            continue;
        }

        const double now {glfwGetTime()};
        m_smoothedFrameSeconds += frameTimeSmoothing * ((now - lastTime) - m_smoothedFrameSeconds);
        lastTime = now;

        handleInput();
        pollShaderFiles(now);

        PanelInfo info {.fps = 1.0 / m_smoothedFrameSeconds,
                        .frameMilliseconds = 1000.0 * m_smoothedFrameSeconds,
                        .traceResolution = m_renderer.traceResolution(),
                        .cameraDistance = static_cast<double>(m_camera.distance()),
                        .glRenderer = m_glRenderer,
                        .shaderError = m_shaderError};
        for (std::size_t pass {0}; pass < gpuPassNames.size(); ++pass) {
            info.gpuMilliseconds[pass] = m_gpuTimer.milliseconds(pass);
        }

        m_gui.beginFrame();
        if (drawControlPanel(m_settings, info).reloadShaders) {
            reloadShaders();
        }
        if (m_settings.vsync != m_vsync) {
            m_vsync = m_settings.vsync;
            m_window.setVsync(m_vsync);
        }
        m_camera.setVerticalFovDegrees(m_settings.fovDegrees);

        m_gpuTimer.beginFrame();
        m_renderer.draw(frameParams(framebuffer, static_cast<float>(now)), m_gpuTimer);
        m_gui.endFrame();
        m_gpuTimer.endSection(index(GpuPass::UI));
        m_window.swapBuffers();
    }
}

void Application::handleInput()
{
    const glm::dvec2 cursor {m_window.cursorPosition()};
    const glm::dvec2 delta {cursor - m_lastCursor};
    m_lastCursor = cursor;

    if (!m_gui.wantsMouse()) {
        if (m_window.isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT)) {
            m_camera.orbit(glm::vec2 {delta});
        }
        m_camera.zoom(m_window.takeScroll());
    } else {
        static_cast<void>(m_window.takeScroll()); // the scroll belonged to an ImGui window
    }

    if (!m_gui.wantsKeyboard()) {
        if (m_window.isKeyDown(GLFW_KEY_ESCAPE)) {
            m_window.requestClose();
        }
        const bool reloadKeyDown {m_window.isKeyDown(GLFW_KEY_R)};
        if (reloadKeyDown && !m_reloadKeyWasDown) {
            reloadShaders();
        }
        m_reloadKeyWasDown = reloadKeyDown;
    }
}

FrameParams Application::frameParams(const glm::ivec2 framebufferSize, const float time) const
{
    const double a {static_cast<double>(m_settings.spin)};
    return FrameParams {
        .outputSize = framebufferSize,
        .renderScale = m_settings.renderScale,
        .cameraPosition = m_camera.position(),
        .cameraBasis = m_camera.basis(),
        .tanHalfFov = std::tan(glm::radians(m_camera.verticalFovDegrees()) / 2.0F),
        .shadowRadius = static_cast<float>(kerr::schwarzschildCriticalImpactParameter),
        .diskEnabled = m_settings.diskEnabled,
        .diskInnerRadius = static_cast<float>(kerr::iscoRadius(a, m_settings.diskOrbit)),
        .diskOuterRadius = m_settings.diskOuterRadius,
        .starsEnabled = m_settings.starsEnabled,
        .time = time,
        .exposure = std::exp2(m_settings.exposureEv),
        .toneMapper = m_settings.toneMapper,
        .bloomEnabled = m_settings.bloomEnabled,
        .bloomStrength = m_settings.bloomStrength,
    };
}

void Application::reloadShaders()
{
    const auto result {m_renderer.reloadShaders()};
    m_shaderError = result ? std::string {} : result.error();
}

void Application::pollShaderFiles(const double now)
{
    if (!m_settings.autoReloadShaders || now < m_nextShaderPoll) {
        return;
    }
    m_nextShaderPoll = now + shaderPollSeconds;
    if (m_renderer.shaderSourcesChanged()) {
        reloadShaders();
    }
}

} // namespace bh
