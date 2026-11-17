#pragma once

#include "assets/SkyImage.hpp"
#include "gfx/GpuTimer.hpp"
#include "gfx/Window.hpp"
#include "physics/Observer.hpp"
#include "render/BlackHoleRenderer.hpp"
#include "scene/OrbitCamera.hpp"
#include "scene/SceneSettings.hpp"
#include "ui/ControlPanel.hpp"
#include "ui/ImGuiLayer.hpp"

#include <glm/glm.hpp>

#include <expected>
#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <vector>

namespace bh {

class Application final {
public:
    Application();

    void run();

private:
    void handleInput();
    [[nodiscard]] FrameParams frameParams(glm::ivec2 framebufferSize) const;
    void drawOverlays() const;

    // The camera's observer this frame: where it is, where it looks, how it moves.
    struct ObserverState {
        glm::vec3 position {};
        glm::mat3 basis {1.0F};
        std::optional<kerr::FourVector> velocity {}; // empty: the ZAMO
    };
    [[nodiscard]] ObserverState observerState() const;
    // Moves orbiting and falling cameras on by `seconds` of wall time.
    void updateObserver(double seconds, bool dropAgain);
    void updateDiskProfile();
    // Starts decoding settings.skyImage on a worker thread when it changes, uploads it when done.
    void updateSkyImage();
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
    ControlPanel m_panel {};
    std::string m_glRenderer {};
    std::string m_shaderError {};
    glm::dvec2 m_lastCursor {};
    bool m_vsync {true};
    bool m_fullscreenKeyWasDown {};
    bool m_escapeLeftFullscreen {}; // Esc still held after leaving fullscreen: do not also quit
    bool m_reloadKeyWasDown {};
    bool m_middleButtonWasDown {};
    bool m_orbitLatched {}; // toggled by a wheel click: orbit without holding a button
    double m_smoothedFrameSeconds {1.0 / 60.0};
    double m_nextShaderPoll {};
    double m_coordinateTime {}; // animation clock in M, advanced at settings.timeScale

    // Free fall: the timelike geodesic of the camera, its proper time, the spin it was integrated with.
    std::optional<kerr::Photon> m_fall {};
    double m_fallProperTime {};
    float m_fallSpin {};
    std::string m_observerNote {};

    // Inputs of the disk temperature profile last uploaded; empty before the first upload.
    struct DiskProfileKey {
        float spin {};
        kerr::Orbit orbit {kerr::Orbit::PROGRADE};
        float outerRadius {};
        bool operator==(const DiskProfileKey&) const = default;
    };
    std::optional<DiskProfileKey> m_diskProfileKey {};

    std::vector<std::filesystem::path> m_skyImages {};  // maps in the asset folder
    std::filesystem::path m_requestedSkyImage {};       // last settings.skyImage a load was started for
    std::future<std::expected<SkyImage, std::string>> m_skyLoad {}; // in flight while valid()
    std::string m_skyStatus {};
    bool m_skyError {};
};

} // namespace bh
