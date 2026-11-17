#pragma once

#include "render/GpuPass.hpp"
#include "scene/SceneSettings.hpp"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace bh {

// Read-only values the panel displays.
struct PanelInfo {
    double fps {};
    double frameMilliseconds {};
    std::array<double, gpuPassNames.size()> gpuMilliseconds {};
    glm::ivec2 traceResolution {};
    std::uint32_t accumulatedSamples {};
    double observerRadius {};             // Boyer-Lindquist r of the camera
    double observerSpeed {};              // relative to the local ZAMO, fraction of c
    std::optional<double> properTime {};  // of a falling camera since it was dropped, M
    std::optional<double> circularSpeed {}; // prograde Keplerian speed at the orbit camera's place, c
    std::string_view observerNote {};
    bool lookCentered {true};
    std::string_view glRenderer {};
    std::string_view shaderError {};
    std::span<const std::filesystem::path> skyImages {}; // maps found in the asset folder
    std::string_view skyStatus {};                       // what is loaded or loading, or why it failed
    bool skyError {};
    std::string_view screenshotStatus {};
    bool screenshotError {};
    std::span<const std::filesystem::path> presets {};
    std::string_view presetStatus {};
    bool presetError {};
};

// What the user asked for this frame.
struct [[nodiscard]] PanelActions {
    bool reloadShaders {};
    bool rescanSkyImages {};
    bool dropAgain {};
    bool recenterLook {};
    bool screenshot {};
    std::optional<std::filesystem::path> loadPreset {};
    std::optional<std::string> savePreset {}; // a name, not yet checked
    bool rescanPresets {};
};

class ControlPanel final {
public:
    PanelActions draw(SceneSettings& settings, const PanelInfo& info);

private:
    void drawCamera(SceneSettings& settings, const PanelInfo& info, PanelActions& actions);
    void drawPresets(const PanelInfo& info, PanelActions& actions);
    void drawPerformance(SceneSettings& settings, const PanelInfo& info);
    void drawBackground(SceneSettings& settings, const PanelInfo& info, PanelActions& actions);

    std::string m_skyPathInput {}; // text field; becomes settings.skyImage on Enter
    std::string m_presetName {};   // text field of "Save"
    float m_uiScaleDraft {1.0F};   // the UI scale slider's value while it is dragged
    bool m_editingUiScale {};
};

} // namespace bh
