#pragma once

#include "scene/SceneSettings.hpp"

#include <string_view>

namespace bh {

// Read-only values the panel displays.
struct PanelInfo {
    double fps {};
    double frameMilliseconds {};
    double cameraDistance {};
    std::string_view glRenderer {};
    std::string_view shaderError {};
};

// What the user asked for this frame.
struct [[nodiscard]] PanelActions {
    bool reloadShaders {};
};

PanelActions drawControlPanel(SceneSettings& settings, const PanelInfo& info);

} // namespace bh
