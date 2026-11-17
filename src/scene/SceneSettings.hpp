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
    float diskPeakTemperature {6500.0F}; // K; scales as (accretion rate / M^2)^(1/4)
    bool dopplerShift {true};            // includes relativistic beaming
    bool gravitationalRedshift {true};   // includes frame dragging

    // Background
    bool starsEnabled {true};

    // Camera
    float fovDegrees {60.0F};

    // Ray tracing
    float stepScale {0.02F};        // near-hole step h = stepScale r^1.5
    float maxStepFraction {0.25F};  // far-field step limit h <= maxStepFraction r
    int maxSteps {600};
    DebugView debugView {DebugView::NONE};
    bool shadowOutline {false};     // overlay of the analytic edge (Bardeen 1973)

    // Image
    float renderScale {0.75F}; // trace resolution / window resolution; the tracer is compute-bound
    ToneMapper toneMapper {ToneMapper::AGX};
    float exposureEv {0.0F}; // exposure = 2^EV
    bool bloomEnabled {true};
    float bloomStrength {0.04F};

    // Performance and development
    bool vsync {true};
    bool autoReloadShaders {true};
};

} // namespace bh
