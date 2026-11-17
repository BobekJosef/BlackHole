#pragma once
// User-tunable parameters, edited by the control panel.

#include "physics/Kerr.hpp"
#include "render/FrameParams.hpp"

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

    // Camera
    float fovDegrees {60.0F};

    // Image
    float renderScale {1.0F}; // trace resolution / window resolution
    ToneMapper toneMapper {ToneMapper::AGX};
    float exposureEv {0.0F}; // exposure = 2^EV
    bool bloomEnabled {true};
    float bloomStrength {0.04F};

    // Performance and development
    bool vsync {true};
    bool autoReloadShaders {true};
};

} // namespace bh
