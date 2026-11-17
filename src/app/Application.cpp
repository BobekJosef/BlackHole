#include "app/Application.hpp"

#include "blackhole/config.hpp"
#include "physics/Blackbody.hpp"
#include "physics/Disk.hpp"
#include "physics/Kerr.hpp"
#include "physics/KerrSchild.hpp"
#include "render/GpuPass.hpp"
#include "ui/ControlPanel.hpp"
#include "ui/ShadowOverlay.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <array>
#include <format>
#include <vector>

namespace bh {

namespace {

// Weight of the newest frame in the exponential moving average of the frame time.
constexpr double frameTimeSmoothing = 0.05;
constexpr double shaderPollSeconds = 1.0;
// Rays count as captured just outside the horizon (the backward integration slows down there).
constexpr double captureMargin = 1.01;
// Sky lookup radius. In outgoing Kerr-Schild coordinates a ray's coordinate direction is within
// ~1e-5 rad of its asymptotic one by r = 200 (tests/test_geodesic.cpp, testEscapeDirection); the
// camera-distance term keeps that true for rays with impact parameters up to the camera distance.
constexpr double minEscapeRadius = 200.0;
constexpr double escapeRadiusPerCameraDistance = 4.0;
constexpr std::size_t shadowOutlineSamples = 512;
constexpr std::size_t diskProfileSamples = 512;

[[nodiscard]] kerr::ThreeVector toDouble(const glm::vec3& v)
{
    return {static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z)};
}

// The ZAMO tetrad at the camera, lowered with the metric and packed as the columns
// (u_mu, right_mu, up_mu, forward_mu) of the shader's uCameraFrame.
[[nodiscard]] glm::mat4 cameraFrame(const glm::vec3& position, const glm::mat3& basis, const double a)
{
    const kerr::ThreeVector x {toDouble(position)};
    const kerr::Tetrad tetrad {
        kerr::zamoTetrad(x, toDouble(basis[0]), toDouble(basis[1]), toDouble(basis[2]), a)};
    const kerr::Metric g {kerr::metric(x, a)};
    glm::mat4 frame {};
    const std::array columns {tetrad.time, tetrad.right, tetrad.up, tetrad.forward};
    for (std::size_t column {0}; column < columns.size(); ++column) {
        const kerr::FourVector lowered {kerr::lower(g, columns[column])};
        for (std::size_t row {0}; row < 4; ++row) {
            frame[static_cast<glm::length_t>(column)][static_cast<glm::length_t>(row)] =
                static_cast<float>(lowered[row]);
        }
    }
    return frame;
}

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
        drawOverlays();

        updateDiskProfile();
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
    const glm::vec3 position {m_camera.position()};
    return FrameParams {
        .outputSize = framebufferSize,
        .renderScale = m_settings.renderScale,
        .cameraPosition = position,
        .cameraFrame = cameraFrame(position, m_camera.basis(), a),
        .tanHalfFov = std::tan(glm::radians(m_camera.verticalFovDegrees()) / 2.0F),
        .spin = m_settings.spin,
        .captureRadius = static_cast<float>(captureMargin * kerr::outerHorizonRadius(a)),
        .diskEnabled = m_settings.diskEnabled,
        .diskInnerRadius = static_cast<float>(kerr::iscoRadius(a, m_settings.diskOrbit)),
        .diskOuterRadius = m_settings.diskOuterRadius,
        .diskPeakTemperature = m_settings.diskPeakTemperature,
        .diskBrightness = static_cast<float>(1.0 / blackbodyLuminance(static_cast<double>(m_settings.diskPeakTemperature))),
        .diskOrbitSign = m_settings.diskOrbit == kerr::Orbit::PROGRADE ? 1.0F : -1.0F,
        .dopplerShift = m_settings.dopplerShift,
        .gravitationalRedshift = m_settings.gravitationalRedshift,
        .starsEnabled = m_settings.starsEnabled,
        .time = time,
        .stepScale = m_settings.stepScale,
        .maxStepFraction = m_settings.maxStepFraction,
        .escapeRadius = static_cast<float>(
            std::max(minEscapeRadius, escapeRadiusPerCameraDistance * static_cast<double>(m_camera.distance()))),
        .maxSteps = m_settings.maxSteps,
        .debugView = m_settings.debugView,
        .exposure = std::exp2(m_settings.exposureEv),
        .toneMapper = m_settings.toneMapper,
        .bloomEnabled = m_settings.bloomEnabled,
        .bloomStrength = m_settings.bloomStrength,
    };
}

void Application::updateDiskProfile()
{
    const DiskProfileKey key {.spin = m_settings.spin, .orbit = m_settings.diskOrbit,
                              .outerRadius = m_settings.diskOuterRadius};
    if (key == m_diskProfileKey) {
        return;
    }
    const auto profile {kerr::diskTemperatureProfile(static_cast<double>(key.spin), key.orbit,
                                                     static_cast<double>(key.outerRadius), diskProfileSamples)};
    std::vector<float> samples(profile.size());
    std::ranges::transform(profile, samples.begin(), [](const double value) { return static_cast<float>(value); });
    m_renderer.setDiskProfile(samples);
    m_diskProfileKey = key;
}

void Application::drawOverlays() const
{
    if (!m_settings.shadowOutline) {
        return;
    }
    const double a {static_cast<double>(m_settings.spin)};
    const kerr::ThreeVector position {toDouble(m_camera.position())};
    const double cameraRadius {kerr::boyerLindquistRadius(position, a)};
    // Boyer-Lindquist inclination: z = r cos(theta) holds exactly in Kerr-Schild coordinates.
    const double inclination {std::acos(std::clamp(position[2] / cameraRadius, -1.0, 1.0))};
    const auto outline {kerr::shadowOutline(a, inclination, shadowOutlineSamples)};
    const double tanHalfFov {std::tan(static_cast<double>(glm::radians(m_camera.verticalFovDegrees())) / 2.0)};
    drawShadowOutline(outline, cameraRadius, tanHalfFov);
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
