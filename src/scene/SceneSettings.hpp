#pragma once
// User-tunable parameters, edited by the control panel.

#include "physics/Kerr.hpp"

namespace bh {

struct SceneSettings {
    // Black hole
    float spin {0.9F}; // a = J/M; the slider stops at Thorne's 0.998
    kerr::Orbit diskOrbit {kerr::Orbit::PROGRADE};

    // Accretion disk (inner edge is the ISCO)
    bool diskEnabled {true};
    float diskOuterRadius {20.0F};

    // Background
    bool starsEnabled {true};

    // Camera and image
    float fovDegrees {60.0F};
    float exposure {1.0F};
    bool vsync {true};
};

} // namespace bh
