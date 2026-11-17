#pragma once
// User-tunable parameters, edited by the control panel.

#include "physics/Kerr.hpp"
#include "render/FrameParams.hpp"

#include <filesystem>

namespace bh {

// What the camera is (physics/Observer.hpp): the frame its image is taken in.
enum class Observer { STATIC, ORBITING, FREE_FALL };

// The background the user picked; an IMAGE is shown once skyImage has loaded.
enum class Background { NONE, STARS, GRID, IMAGE };

struct SceneSettings {
    // Black hole
    float spin {0.1F}; // a = J/M; the slider stops at Thorne's 0.998
    kerr::Orbit diskOrbit {kerr::Orbit::PROGRADE};

    // Accretion disk (inner edge is the ISCO)
    bool diskEnabled {true};
    // Far enough out that the disk has cooled below visibility (T ~ r^-3/4): it fades instead of ending.
    float diskOuterRadius {60.0F};
    float diskEdgeSoftness {0.4F};       // outer fraction of the radius over which the disk thins out
    float diskPeakTemperature {5000.0F}; // K; scales as (accretion rate / M^2)^(1/4)
    bool dopplerShift {true};            // includes relativistic beaming
    bool gravitationalRedshift {true};   // includes frame dragging
    bool limbDarkening {true};           // electron-scattering atmosphere
    float diskTurbulence {0.5F};         // sigma of ln F (MRI-like model); 0 = smooth Novikov-Thorne
    float diskEddySize {0.05F};          // eddy size / r, ~ the disk's aspect ratio H / r

    // Background
    Background background {Background::STARS};
    std::filesystem::path skyImage {};   // equirectangular file or cube-map directory (SkyImage.hpp)
    float skyBrightnessEv {0.0F};
    float skyYawDegrees {180.0F};        // about the spin axis; 180 puts a map's centre behind the hole
    float skyTiltDegrees {0.0F};         // tilts the map's pole: about the world x axis, after the yaw

    // Camera
    Observer observer {Observer::STATIC};
    float fovDegrees {60.0F};
    // Clock of everything that moves (disk turbulence, orbiting and falling cameras): M per second; 0 = frozen.
    float timeScale {4.0F};

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
    bool fullscreen {false};
    bool autoReloadShaders {true};
};

} // namespace bh
