#pragma once
// Everything the renderer needs for one frame, already converted to GPU-friendly floats.

#include <glm/glm.hpp>

namespace bh {

// The values are the TONE_MAPPER_* constants in shaders/common/color.glsl.
enum class ToneMapper { AGX = 0, ACES = 1, EXPONENTIAL = 2, CLAMP = 3 };

// The values are the DEBUG_VIEW_* constants in shaders/common/frame.glsl.
enum class DebugView { NONE = 0, STEPS = 1, CONSTRAINT = 2, FATE = 3, REDSHIFT = 4, MAGNIFICATION = 5 };

// What escaped rays see. The values are the SKY_* constants in shaders/common/frame.glsl.
enum class Sky { NONE = 0, STARS = 1, GRID = 2, EQUIRECTANGULAR = 3, CUBEMAP = 4 };

struct FrameParams {
    // Output and resolution
    glm::ivec2 outputSize {};  // window framebuffer in pixels
    float renderScale {1.0F};  // trace resolution / output resolution

    // Camera
    glm::vec3 cameraPosition {};   // Kerr-Schild Cartesian, units of M
    glm::mat4 cameraFrame {1.0F};  // covariant ZAMO tetrad, columns u, right, up, forward
    float tanHalfFov {};

    // Spacetime and scene
    float spin {};
    float captureRadius {};
    bool diskEnabled {};
    float diskInnerRadius {};
    float diskOuterRadius {};
    float diskPeakTemperature {};  // K
    float diskBrightness {};       // 1 / luminance of the peak temperature
    float diskOrbitSign {1.0F};    // +1 prograde, -1 retrograde
    bool dopplerShift {true};
    bool gravitationalRedshift {true};
    bool limbDarkening {true};
    float diskTurbulence {};       // sigma of ln F; 0 is the smooth Novikov-Thorne disk
    float diskEddySize {0.05F};    // eddy size / r (~ H / r)
    float diskEdgeSoftness {};     // fraction of r_out over which the disk thins out (0: sharp edge)
    float time {};                 // animation clock: coordinate time in M

    // Background
    Sky sky {Sky::STARS};           // EQUIRECTANGULAR / CUBEMAP only once the renderer holds such a map
    glm::mat3 skyRotation {1.0F};   // world direction -> sky map direction
    float skyBrightness {1.0F};     // linear factor

    // Integrator
    float stepScale {};
    float maxStepFraction {};
    float escapeRadius {};
    int maxSteps {};
    DebugView debugView {DebugView::NONE};

    // Temporal accumulation: average jittered frames while the trace inputs do not change
    bool accumulate {true};

    // Post-processing
    bool autoExposure {};
    float frameSeconds {}; // wall time since the last frame (exposure adaptation); 0 freezes it
    float exposure {1.0F}; // linear factor (on top of the automatic one when that is on)
    ToneMapper toneMapper {ToneMapper::AGX};
    bool bloomEnabled {};
    float bloomStrength {};
};

} // namespace bh
