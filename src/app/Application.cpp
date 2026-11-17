#include "app/Application.hpp"

#include "app/Validation.hpp"
#include "blackhole/config.hpp"
#include "physics/Blackbody.hpp"
#include "physics/Disk.hpp"
#include "physics/Kerr.hpp"
#include "physics/KerrSchild.hpp"
#include "render/GpuPass.hpp"
#include "scene/Preset.hpp"
#include "ui/ControlPanel.hpp"
#include "ui/ShadowOverlay.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>

namespace bh {

namespace {

// Weight of the newest frame in the exponential moving average of the frame time.
constexpr double frameTimeSmoothing = 0.05;
constexpr double shaderPollSeconds = 1.0;
constexpr double doubleClickSeconds = 0.3; // as Dear ImGui's default
// Orbiting latched by a wheel click turns slower than a drag: the hand moves freely, without a button
// held and without screen edges, so the same speed felt too twitchy.
constexpr float latchedOrbitSensitivity = 0.35F;
// Rays count as captured just outside the horizon (the backward integration slows down there).
constexpr double captureMargin = 1.01;
// Sky lookup radius. In outgoing Kerr-Schild coordinates a ray's coordinate direction is within
// ~1e-5 rad of its asymptotic one by r = 200 (tests/test_geodesic.cpp, testEscapeDirection); the
// camera-distance term keeps that true for rays with impact parameters up to the camera distance.
constexpr double minEscapeRadius = 200.0;
constexpr double escapeRadiusPerCameraDistance = 4.0;
constexpr std::size_t shadowOutlineSamples = 512;
// A falling camera stops here: outgoing Kerr-Schild coordinates end at the future horizon.
constexpr double fallStopHorizonFactor = 1.05;
// A camera launched faster than escape stops here (the orbit camera's largest distance).
constexpr double fallEscapeRadius = 1000.0;
// |z| / r below which the camera counts as in the disk plane (for a note in the panel).
constexpr double diskPlaneMargin = 0.002;
// The u^2 grid puts ~sqrt(r_peak / r_out) of the samples inside the flux peak: 2048 keep it smooth up to
// r_out = 1000 M.
constexpr std::size_t diskProfileSamples = 2048;
// Rays must reach the whole disk before they count as escaped.
constexpr double escapeRadiusPerDiskRadius = 1.05;

// Set by SIGINT / SIGTERM (Ctrl+C in the terminal): the frame loop then closes the window and the app
// exits normally (destructors run, sanitizers report).
volatile std::sig_atomic_t g_stopRequested {0};

void requestStop(const int /*signal*/)
{
    g_stopRequested = 1;
}

[[nodiscard]] kerr::ThreeVector toDouble(const glm::vec3& v)
{
    return {static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z)};
}

// The camera's tetrad (around `velocity`, or the ZAMO's if empty), lowered with the metric and packed as
// the columns (u_mu, right_mu, up_mu, forward_mu) of the shader's uCameraFrame.
[[nodiscard]] glm::mat4 cameraFrame(const glm::vec3& position, const glm::mat3& basis,
                                    const std::optional<kerr::FourVector>& velocity, const double a)
{
    const kerr::ThreeVector x {toDouble(position)};
    const kerr::Tetrad tetrad {kerr::observerTetrad(x, velocity.value_or(kerr::zamoVelocity(x, a)), toDouble(basis[0]),
                                                    toDouble(basis[1]), toDouble(basis[2]), a)};
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

// What the panel says about a loaded map.
[[nodiscard]] std::string describe(const std::filesystem::path& path, const SkyImage& image)
{
    const SkyFace& face {image.faces.front()};
    const bool cube {image.projection == SkyProjection::CUBEMAP};
    return std::format("{}: {} {}x{}, {}", path.filename().string(), cube ? "cube map" : "equirectangular", face.width,
                       face.height, face.isHdr() ? "linear float" : "8-bit sRGB");
}

// A relative path that does not exist from the working directory is looked up in the asset folder.
[[nodiscard]] std::filesystem::path resolveAssetPath(const std::filesystem::path& path)
{
    std::error_code error {};
    if (path.is_absolute() || std::filesystem::exists(path, error)) {
        return path;
    }
    return std::filesystem::path {config::assetDir} / path;
}

[[nodiscard]] Sky frameSky(const Background background, const std::optional<SkyProjection> loaded)
{
    switch (background) {
    case Background::NONE: return Sky::NONE;
    case Background::STARS: return Sky::STARS;
    case Background::GRID: return Sky::GRID;
    case Background::IMAGE:
        if (!loaded) {
            return Sky::NONE;
        }
        return *loaded == SkyProjection::CUBEMAP ? Sky::CUBEMAP : Sky::EQUIRECTANGULAR;
    }
    return Sky::NONE;
}

// Unit Boyer-Lindquist radial direction d/dr at `position` (constant t, theta, Kerr-Schild phi) in Kerr-Schild
// Cartesian components: x + iy = (r + ia) e^{i phi} sin(theta), so it is (x + iy) / (r + ia) with z / r.
// Near a spinning hole the position vector itself is twisted against it by atan(a / r) (31 deg at r = 1.5,
// a = 0.9), which put a falling camera's view of the hole off-centre.
[[nodiscard]] glm::vec3 radialDirection(const kerr::ThreeVector& position, const double a)
{
    const auto [x, y, z] {position};
    const double r {kerr::boyerLindquistRadius(position, a)};
    const double s {r * r + a * a};
    return glm::normalize(glm::vec3 {static_cast<float>((x * r + y * a) / s), static_cast<float>((y * r - x * a) / s),
                                     static_cast<float>(z / r)});
}

[[nodiscard]] std::string glString(const GLenum name)
{
    // glGetString returns const GLubyte*; the bytes are a NUL-terminated character string.
    return reinterpret_cast<const char*>(glGetString(name));
}

} // namespace

Application::Application(const ApplicationOptions& options)
    : m_window {WindowDesc {.title = std::format("BlackHole {}", config::projectVersion),
                            .vsync = true,
                            .debugContext = config::debugBuild,
                            .visible = !options.hiddenWindow}}
    , m_gui {m_window.handle()}
    , m_renderer {std::filesystem::path {config::shaderDir}}
    , m_gpuTimer {gpuPassNames.size()}
    , m_glRenderer {glString(GL_RENDERER) + " | OpenGL " + glString(GL_VERSION)}
    , m_lastCursor {m_window.cursorPosition()}
    , m_skyImages {findSkyImages(std::filesystem::path {config::assetDir})}
    , m_presets {findPresets(std::filesystem::path {config::presetDir})}
{
}

void Application::run()
{
    // Should installing a handler fail, Ctrl+C still ends the app, only without the orderly shutdown.
    static_cast<void>(std::signal(SIGINT, requestStop));
    static_cast<void>(std::signal(SIGTERM, requestStop));
    double lastTime {glfwGetTime()};
    while (!m_window.shouldClose()) {
        glfwPollEvents();
        if (g_stopRequested != 0) {
            m_window.requestClose();
            continue;
        }

        const glm::ivec2 framebuffer {m_window.framebufferSize()};
        if (framebuffer.x <= 0 || framebuffer.y <= 0) { // minimised
            glfwWaitEvents();
            continue;
        }

        const double now {glfwGetTime()};
        const double seconds {now - lastTime};
        m_smoothedFrameSeconds += frameTimeSmoothing * (seconds - m_smoothedFrameSeconds);
        m_frameSeconds = seconds;
        m_coordinateTime += static_cast<double>(m_settings.timeScale) * seconds;
        lastTime = now;

        handleInput();
        pollShaderFiles(now);

        const ObserverState observer {observerState()};
        const double a {static_cast<double>(m_settings.spin)};
        const kerr::ThreeVector observerPosition {toDouble(observer.position)};
        // Guide for the free-fall launch: the prograde circular speed where the next drop starts.
        const kerr::ThreeVector dropPosition {toDouble(m_camera.position())};
        const auto circular {kerr::keplerianVelocity(dropPosition, a, kerr::Orbit::PROGRADE)};
        const std::optional<double> circularSpeed {
            circular ? std::optional<double> {kerr::speedRelativeToZamo(dropPosition, *circular, a)} : std::nullopt};
        PanelInfo info {.fps = 1.0 / m_smoothedFrameSeconds,
                        .frameMilliseconds = 1000.0 * m_smoothedFrameSeconds,
                        .traceResolution = m_renderer.traceResolution(),
                        .accumulatedSamples = m_renderer.accumulatedSamples(),
                        .observerRadius = kerr::boyerLindquistRadius(observerPosition, a),
                        .observerSpeed = observer.velocity
                            ? kerr::speedRelativeToZamo(observerPosition, *observer.velocity, a)
                            : 0.0,
                        .properTime = m_fall ? std::optional<double> {m_fallProperTime} : std::nullopt,
                        .circularSpeed = circularSpeed,
                        .observerNote = m_observerNote,
                        .lookCentered = m_camera.isLookCentered(),
                        .glRenderer = m_glRenderer,
                        .shaderError = m_shaderError,
                        .skyImages = m_skyImages,
                        .skyStatus = m_skyStatus,
                        .skyError = m_skyError,
                        .screenshotStatus = m_screenshotStatus,
                        .screenshotError = m_screenshotError,
                        .presets = m_presets,
                        .presetStatus = m_presetStatus,
                        .presetError = m_presetError};
        for (std::size_t pass {0}; pass < gpuPassNames.size(); ++pass) {
            info.gpuMilliseconds[pass] = m_gpuTimer.milliseconds(pass);
        }

        m_gui.setScale(m_window.contentScale() * m_settings.uiScale);
        m_gui.beginFrame();
        const PanelActions actions {m_panel.draw(m_settings, info)};
        if (actions.reloadShaders) {
            reloadShaders();
        }
        m_screenshotRequested = m_screenshotRequested || actions.screenshot;
        if (actions.recenterLook) {
            m_camera.recenterLook();
        }
        if (actions.loadPreset) {
            loadPresetFile(*actions.loadPreset);
        }
        if (actions.savePreset) {
            savePresetAs(*actions.savePreset);
        }
        if (actions.rescanPresets) {
            m_presets = findPresets(std::filesystem::path {config::presetDir});
        }
        if (actions.rescanSkyImages) {
            m_skyImages = findSkyImages(std::filesystem::path {config::assetDir});
        }
        if (m_settings.vsync != m_vsync) {
            m_vsync = m_settings.vsync;
            m_window.setVsync(m_vsync);
        }
        if (m_settings.fullscreen != m_window.isFullscreen()) {
            m_window.setFullscreen(m_settings.fullscreen);
        }
        m_camera.setVerticalFovDegrees(m_settings.fovDegrees);
        updateObserver(seconds, actions.dropAgain);
        drawOverlays();

        updateDiskProfile();
        updateSkyImage();
        updateScreenshotStatus();
        if (m_screenshotRequested) {
            m_screenshotRequested = false;
            takeScreenshot(framebuffer);
        }
        m_gpuTimer.beginFrame();
        m_renderer.draw(frameParams(framebuffer), m_gpuTimer);
        m_gui.endFrame();
        m_gpuTimer.endSection(index(GpuPass::UI));
        m_window.swapBuffers();
    }
}

int Application::validate()
{
    struct Scene {
        std::string_view name {};
        float spin {};
        CameraPlacement camera {};
        float diskOuterRadius {};
    };
    const std::array scenes {
        Scene {.name = "Kerr a = 0.9, nearly edge-on, 30 M",
               .spin = 0.9F,
               .camera = CameraPlacement {.polarDegrees = 80.0F, .distance = 30.0F},
               .diskOuterRadius = 20.0F},
        Scene {.name = "Kerr a = 0.5, nearly face-on, 25 M",
               .spin = 0.5F,
               .camera = CameraPlacement {.azimuthDegrees = 40.0F, .polarDegrees = 15.0F, .distance = 25.0F},
               .diskOuterRadius = 30.0F},
        Scene {.name = "Schwarzschild, edge-on, 12 M",
               .spin = 0.0F,
               .camera = CameraPlacement {.polarDegrees = 88.0F, .distance = 12.0F},
               .diskOuterRadius = 40.0F},
    };
    constexpr glm::ivec2 size {160, 90};

    bool allPassed {true};
    for (const Scene& scene : scenes) {
        m_settings = SceneSettings {};
        m_settings.spin = scene.spin;
        m_settings.diskOuterRadius = scene.diskOuterRadius;
        m_settings.diskEdgeSoftness = 0.0F; // an opaque disk, as in the reference
        m_camera.place(scene.camera);
        updateDiskProfile();
        const FrameParams frame {frameParams(size)};
        const std::vector<glm::vec4> gpu {m_renderer.traceForValidation(frame)};

        // The reference gets the same (float) inputs, in double.
        const double a {static_cast<double>(frame.spin)};
        const ObserverState observer {observerState()};
        const kerr::ThreeVector position {toDouble(observer.position)};
        const ReferenceSetup setup {
            .spin = a,
            .cameraPosition = position,
            .cameraTetrad = kerr::zamoTetrad(position, toDouble(observer.basis[0]), toDouble(observer.basis[1]),
                                             toDouble(observer.basis[2]), a),
            .tanHalfFov = static_cast<double>(frame.tanHalfFov),
            .width = size.x,
            .height = size.y,
            .trace =
                kerr::TraceSettings {
                    .stepScale = static_cast<double>(frame.stepScale),
                    .maxStepFraction = static_cast<double>(frame.maxStepFraction),
                    .maxSteps = frame.maxSteps,
                    .escapeRadius = static_cast<double>(frame.escapeRadius),
                    .horizonMargin = static_cast<double>(frame.captureRadius) / kerr::outerHorizonRadius(a),
                    .disk = kerr::DiskExtent {.innerRadius = static_cast<double>(frame.diskInnerRadius),
                                              .outerRadius = static_cast<double>(frame.diskOuterRadius)}},
        };
        const ValidationResult result {compareWithReference(scene.name, setup, gpu)};
        std::cout << result.report;
        allPassed = allPassed && result.passed;
    }
    // Flushed: a sanitizer ending the process at exit would drop buffered output.
    std::cout << (allPassed ? "validation passed" : "validation FAILED") << '\n';
    std::cout.flush();
    return allPassed ? 0 : 1;
}

void Application::handleInput()
{
    const glm::dvec2 cursor {m_window.cursorPosition()};
    const glm::dvec2 delta {cursor - m_lastCursor};
    m_lastCursor = cursor;

    // A click of the wheel (middle button) latches orbiting on, as if the left button were held, until
    // the next wheel click, a left click or Esc. While latched the cursor is captured: hidden, and not
    // stopped by the screen edges.
    const bool leftDown {m_window.isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT)};
    const bool middleDown {m_window.isMouseButtonDown(GLFW_MOUSE_BUTTON_MIDDLE)};
    const bool middlePressed {middleDown && !m_middleButtonWasDown};
    m_middleButtonWasDown = middleDown;

    // Right button: look around, in every camera mode; a double click looks back at the hole.
    const bool rightDown {m_window.isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT)};
    const bool rightPressed {rightDown && !m_rightButtonWasDown};
    m_rightButtonWasDown = rightDown;
    if (!m_gui.wantsMouse()) {
        if (rightPressed) {
            const double now {glfwGetTime()};
            if (now - m_lastRightPress < doubleClickSeconds) {
                m_camera.recenterLook();
            }
            m_lastRightPress = now;
        } else if (rightDown) {
            m_camera.look(glm::vec2 {delta});
        }
    }

    // Orbiting and zooming move the camera; a falling one goes where gravity takes it.
    if (!m_gui.wantsMouse() && m_settings.observer != Observer::FREE_FALL) {
        if (middlePressed) {
            m_orbitLatched = !m_orbitLatched;
        }
        if (leftDown) {
            m_orbitLatched = false;
        }
        if (leftDown) {
            m_camera.orbit(glm::vec2 {delta});
        } else if (m_orbitLatched) {
            m_camera.orbit(latchedOrbitSensitivity * glm::vec2 {delta});
        }
        m_camera.zoom(m_window.takeScroll());
    } else {
        static_cast<void>(m_window.takeScroll()); // the scroll belonged to an ImGui window
        if (m_settings.observer == Observer::FREE_FALL) {
            m_orbitLatched = false; // a falling camera cannot be orbited
        }
    }

    if (!m_gui.wantsKeyboard()) {
        // Esc releases a latched orbit (and the cursor) first, then leaves fullscreen, then quits.
        if (m_window.isKeyDown(GLFW_KEY_ESCAPE)) {
            if (m_orbitLatched) {
                m_orbitLatched = false;
                m_escapeLeftFullscreen = true; // the same press must not also go on to quit
            } else if (m_settings.fullscreen) {
                m_settings.fullscreen = false;
                m_escapeLeftFullscreen = true;
            } else if (!m_escapeLeftFullscreen) {
                m_window.requestClose();
            }
        } else {
            m_escapeLeftFullscreen = false;
        }
        const bool screenshotKeyDown {m_window.isKeyDown(GLFW_KEY_F12)};
        m_screenshotRequested = m_screenshotRequested || (screenshotKeyDown && !m_screenshotKeyWasDown);
        m_screenshotKeyWasDown = screenshotKeyDown;
        const bool fullscreenKeyDown {m_window.isKeyDown(GLFW_KEY_F11)};
        if (fullscreenKeyDown && !m_fullscreenKeyWasDown) {
            m_settings.fullscreen = !m_settings.fullscreen;
        }
        m_fullscreenKeyWasDown = fullscreenKeyDown;
        if (m_window.isKeyDown(GLFW_KEY_C)) {
            m_camera.recenterLook();
        }
        const bool reloadKeyDown {m_window.isKeyDown(GLFW_KEY_R)};
        if (reloadKeyDown && !m_reloadKeyWasDown) {
            reloadShaders();
        }
        m_reloadKeyWasDown = reloadKeyDown;
    }

    if (m_orbitLatched != m_window.isCursorCaptured()) {
        m_window.setCursorCaptured(m_orbitLatched);
        m_gui.setMouseEnabled(!m_orbitLatched);
        m_lastCursor = m_window.cursorPosition(); // capturing moves the reported position: no jump
    }
}

FrameParams Application::frameParams(const glm::ivec2 framebufferSize) const
{
    const double a {static_cast<double>(m_settings.spin)};
    const ObserverState observer {observerState()};
    const glm::vec3 position {observer.position};
    const double diskReach {
        m_settings.diskEnabled ? escapeRadiusPerDiskRadius * static_cast<double>(m_settings.diskOuterRadius) : 0.0};
    return FrameParams {
        .outputSize = framebufferSize,
        .renderScale = m_settings.renderScale,
        .cameraPosition = position,
        .cameraFrame = cameraFrame(position, observer.basis, observer.velocity, a),
        .tanHalfFov = std::tan(glm::radians(m_camera.verticalFovDegrees()) / 2.0F),
        .spin = m_settings.spin,
        .captureRadius = static_cast<float>(captureMargin * kerr::outerHorizonRadius(a)),
        .diskEnabled = m_settings.diskEnabled,
        .diskInnerRadius = static_cast<float>(kerr::iscoRadius(a, m_settings.diskOrbit)),
        .diskOuterRadius = m_settings.diskOuterRadius,
        .diskPeakTemperature = m_settings.diskPeakTemperature,
        .diskBrightness =
            static_cast<float>(1.0 / blackbodyLuminance(static_cast<double>(m_settings.diskPeakTemperature))),
        .diskOrbitSign = m_settings.diskOrbit == kerr::Orbit::PROGRADE ? 1.0F : -1.0F,
        .dopplerShift = m_settings.dopplerShift,
        .gravitationalRedshift = m_settings.gravitationalRedshift,
        .limbDarkening = m_settings.limbDarkening,
        .diskTurbulence = m_settings.diskTurbulence,
        .diskEddySize = m_settings.diskEddySize,
        .diskEdgeSoftness = m_settings.diskEdgeSoftness,
        // The clock only drives the turbulence; without it a still view keeps accumulating.
        .time =
            m_settings.diskEnabled && m_settings.diskTurbulence > 0.0F ? static_cast<float>(m_coordinateTime) : 0.0F,
        .sky = frameSky(m_settings.background, m_renderer.skyProjection()),
        // Yaw about the spin axis, then tilt about x.
        .skyRotation = glm::mat3 {glm::rotate(
            glm::rotate(glm::mat4 {1.0F}, glm::radians(m_settings.skyTiltDegrees), glm::vec3 {1.0F, 0.0F, 0.0F}),
            glm::radians(m_settings.skyYawDegrees), glm::vec3 {0.0F, 0.0F, 1.0F})},
        .skyBrightness = std::exp2(m_settings.skyBrightnessEv),
        .stepScale = m_settings.stepScale,
        .maxStepFraction = m_settings.maxStepFraction,
        .escapeRadius = static_cast<float>(std::max(
            {minEscapeRadius, escapeRadiusPerCameraDistance * static_cast<double>(glm::length(position)), diskReach})),
        .maxSteps = m_settings.maxSteps,
        .debugView = m_settings.debugView,
        .accumulate = m_settings.refineStill,
        .autoExposure = m_settings.autoExposure,
        .frameSeconds = static_cast<float>(m_frameSeconds),
        .exposure = std::exp2(m_settings.exposureEv),
        .toneMapper = m_settings.toneMapper,
        .bloomEnabled = m_settings.bloomEnabled,
        .bloomStrength = m_settings.bloomStrength,
    };
}

void Application::updateDiskProfile()
{
    const DiskProfileKey key {
        .spin = m_settings.spin, .orbit = m_settings.diskOrbit, .outerRadius = m_settings.diskOuterRadius};
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

void Application::updateSkyImage()
{
    if (m_skyLoad.valid()) {
        if (m_skyLoad.wait_for(std::chrono::seconds {0}) != std::future_status::ready) {
            return;
        }
        auto image {m_skyLoad.get()};
        const auto uploaded {image ? m_renderer.setSkyImage(*image) : std::unexpected {std::move(image.error())}};
        m_skyError = !uploaded;
        m_skyStatus = uploaded ? describe(m_requestedSkyImage, *image) : uploaded.error();
    }
    // One load at a time: a choice made meanwhile starts when the current one is done.
    if (m_settings.background != Background::IMAGE || m_settings.skyImage.empty()
        || m_settings.skyImage == m_requestedSkyImage) {
        return;
    }
    m_requestedSkyImage = m_settings.skyImage;
    m_skyStatus = std::format("Loading {}...", m_requestedSkyImage.filename().string());
    m_skyError = false;
    m_skyLoad =
        std::async(std::launch::async, [path = resolveAssetPath(m_requestedSkyImage)] { return loadSkyImage(path); });
}

auto Application::observerState() const -> ObserverState
{
    const double a {static_cast<double>(m_settings.spin)};
    if (m_settings.observer == Observer::FREE_FALL && m_fall) {
        const auto& [x, y, z] {m_fall->position};
        const glm::vec3 position {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
        return ObserverState {.position = position,
                              .basis =
                                  m_camera.applyLook(OrbitCamera::lookAlong(-radialDirection(m_fall->position, a))),
                              .velocity = kerr::observerVelocity(*m_fall, a)};
    }
    ObserverState state {
        .position = m_camera.position(), .basis = m_camera.applyLook(m_camera.basis()), .velocity = std::nullopt};
    if (m_settings.observer == Observer::ORBITING) {
        state.velocity = kerr::keplerianVelocity(toDouble(state.position), a, m_settings.diskOrbit);
    }
    return state;
}

void Application::updateObserver(const double seconds, const bool dropAgain)
{
    const double a {static_cast<double>(m_settings.spin)};
    const double clock {static_cast<double>(m_settings.timeScale) * seconds}; // M
    m_observerNote.clear();
    if (m_settings.observer != Observer::FREE_FALL) {
        m_fall.reset();
    }

    // A camera within the disk's radii and (almost) in its plane sees the infinitely thin, opaque disk fill
    // the half of the sky on the other side of it: physical, but it looks like a bug without a word.
    if (m_settings.observer != Observer::FREE_FALL && m_settings.diskEnabled) {
        const glm::vec3 position {m_camera.position()};
        const double r {kerr::boyerLindquistRadius(toDouble(position), a)};
        if (std::abs(static_cast<double>(position.z)) < diskPlaneMargin * r
            && r <= static_cast<double>(m_settings.diskOuterRadius)) {
            m_observerNote = "In the disk plane: the thin disk fills the sky above or below. Tilt the camera a little.";
        }
    }

    switch (m_settings.observer) {
    case Observer::STATIC: break;
    case Observer::ORBITING: {
        // Carried around at Omega in coordinate time, the clock the disk turns on too.
        const kerr::ThreeVector position {toDouble(m_camera.position())};
        if (kerr::keplerianVelocity(position, a, m_settings.diskOrbit)) {
            const double r {kerr::boyerLindquistRadius(position, a)};
            m_camera.turn(static_cast<float>(kerr::keplerianAngularVelocity(r, a, m_settings.diskOrbit) * clock));
        } else {
            m_observerNote = std::format("No circular orbit inside the photon orbit (r = {:.2f} M): static instead.",
                                         kerr::photonOrbitRadius(a, m_settings.diskOrbit));
        }
        break;
    }
    case Observer::FREE_FALL: {
        // Dropped from the orbit camera's place; the clock runs in the camera's proper time.
        if (!m_fall || dropAgain || m_fallSpin != m_settings.spin || m_fallSpeed != m_settings.fallSpeed) {
            m_fall =
                kerr::launchedFromZamo(toDouble(m_camera.position()), a, static_cast<double>(m_settings.fallSpeed));
            m_fallProperTime = 0.0;
            m_fallSpin = m_settings.spin;
            m_fallSpeed = m_settings.fallSpeed;
        }
        if (kerr::boyerLindquistRadius(m_fall->position, a) > fallEscapeRadius) {
            m_observerNote = std::format("Escaped beyond {:.0f} M. Drop again to restart.", fallEscapeRadius);
            break;
        }
        const double stop {fallStopHorizonFactor * kerr::outerHorizonRadius(a)};
        const kerr::ProperTimeStep step {kerr::advanceProperTime(*m_fall, a, clock, stop)};
        m_fall = step.state;
        m_fallProperTime += step.elapsed;
        if (kerr::boyerLindquistRadius(m_fall->position, a) <= stop) {
            m_observerNote = std::format("Stopped at r = {:.2f} M, just outside the horizon: the coordinates end "
                                         "there. Drop again to restart.",
                                         stop);
        }
        break;
    }
    }
}

void Application::takeScreenshot(const glm::ivec2 windowSize)
{
    using enum ScreenshotSize;
    const glm::ivec2 size {[&] {
        switch (m_settings.screenshotSize) {
        case WINDOW: return windowSize;
        case FULL_HD: return glm::ivec2 {1920, 1080};
        case QHD: return glm::ivec2 {2560, 1440};
        case UHD_4K: return glm::ivec2 {3840, 2160};
        case UHD_8K: return glm::ivec2 {7680, 4320};
        }
        return windowSize;
    }()};
    FrameParams frame {frameParams(size)};
    frame.renderScale = 1.0F; // full quality, whatever the interactive setting
    auto image {m_renderer.renderImage(frame, static_cast<std::uint32_t>(std::max(m_settings.screenshotSamples, 1)))};
    if (!image) {
        m_screenshotStatus = image.error();
        m_screenshotError = true;
        return;
    }
    if (m_screenshotSave.valid()) {
        m_screenshotSave.wait(); // one save at a time; rare, and its result is superseded by this one
    }
    const std::filesystem::path path {timestampedPath(std::filesystem::path {config::screenshotDir}, "blackhole")};
    m_screenshotStatus = std::format("Saving {} ({}x{})...", path.filename().string(), size.x, size.y);
    m_screenshotError = false;
    m_screenshotSave = std::async(
        std::launch::async, [path, image = std::move(*image)] -> std::expected<std::filesystem::path, std::string> {
            if (auto written {writePng(path, image)}; !written) {
                return std::unexpected {std::move(written.error())};
            }
            return path;
        });
}

void Application::updateScreenshotStatus()
{
    if (!m_screenshotSave.valid() || m_screenshotSave.wait_for(std::chrono::seconds {0}) != std::future_status::ready) {
        return;
    }
    const auto saved {m_screenshotSave.get()};
    m_screenshotError = !saved;
    m_screenshotStatus = saved ? std::format("Saved {}", saved->string()) : saved.error();
}

void Application::loadPresetFile(const std::filesystem::path& path)
{
    const auto parsed {loadPreset(path, Preset {.settings = m_settings, .camera = m_camera.placement()})};
    m_presetError = !parsed;
    if (!parsed) {
        m_presetStatus = parsed.error();
        return;
    }
    m_settings = parsed->preset.settings;
    m_camera.place(parsed->preset.camera);
    m_fall.reset(); // a falling camera starts again from the preset's place
    m_presetStatus = std::format("Loaded {}", path.stem().string());
    for (const std::string& warning : parsed->warnings) {
        m_presetStatus += "\n" + warning;
    }
}

void Application::savePresetAs(const std::string_view name)
{
    m_presetError = true;
    if (!isValidPresetName(name)) {
        m_presetStatus = "A preset name is letters, digits, spaces, '-' and '_'.";
        return;
    }
    const std::filesystem::path path {std::filesystem::path {config::presetDir}
                                      / (std::string {name} + std::string {presetExtension})};
    std::error_code error {};
    const bool replaced {std::filesystem::exists(path, error)};
    if (const auto saved {savePreset(path, Preset {.settings = m_settings, .camera = m_camera.placement()})}; !saved) {
        m_presetStatus = saved.error();
        return;
    }
    m_presetError = false;
    m_presetStatus = std::format("{} {}", replaced ? "Replaced" : "Saved", path.filename().string());
    m_presets = findPresets(std::filesystem::path {config::presetDir});
}

void Application::drawOverlays() const
{
    // The analytic edge is that of a static observer (the ZAMO) looking at the hole; a moving camera sees
    // it aberrated, and looking away moves it off the centre the overlay assumes.
    if (!m_settings.shadowOutline || m_settings.observer != Observer::STATIC || !m_camera.isLookCentered()) {
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
