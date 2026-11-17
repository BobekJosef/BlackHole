#pragma once

#include "render/GpuPass.hpp"
#include "scene/SceneSettings.hpp"

#include <glm/glm.hpp>

#include <array>
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
    double observerRadius {};             // Boyer-Lindquist r of the camera
    double observerSpeed {};              // relative to the local ZAMO, fraction of c
    std::optional<double> properTime {};  // of a falling camera since it was dropped, M
    std::string_view observerNote {};
    std::string_view glRenderer {};
    std::string_view shaderError {};
    std::span<const std::filesystem::path> skyImages {}; // maps found in the asset folder
    std::string_view skyStatus {};                       // what is loaded or loading, or why it failed
    bool skyError {};
};

// What the user asked for this frame.
struct [[nodiscard]] PanelActions {
    bool reloadShaders {};
    bool rescanSkyImages {};
    bool dropAgain {};
};

class ControlPanel final {
public:
    PanelActions draw(SceneSettings& settings, const PanelInfo& info);

private:
    void drawCamera(SceneSettings& settings, const PanelInfo& info, PanelActions& actions);
    void drawBackground(SceneSettings& settings, const PanelInfo& info, PanelActions& actions);

    std::string m_skyPathInput {}; // text field; becomes settings.skyImage on Enter
};

} // namespace bh
