#pragma once
// Everything the renderer needs for one frame, already converted to GPU-friendly floats.

#include <glm/glm.hpp>

namespace bh {

// The values are the TONE_MAPPER_* constants in shaders/common/color.glsl.
enum class ToneMapper { AGX = 0, ACES = 1, EXPONENTIAL = 2, CLAMP = 3 };

// The values are the DEBUG_VIEW_* constants in shaders/common/frame.glsl.
enum class DebugView { NONE = 0, STEPS = 1, CONSTRAINT = 2, FATE = 3 };

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
    bool starsEnabled {};
    float time {};

    // Integrator
    float stepScale {};
    float maxStepFraction {};
    float escapeRadius {};
    int maxSteps {};
    DebugView debugView {DebugView::NONE};

    // Post-processing
    float exposure {1.0F}; // linear factor
    ToneMapper toneMapper {ToneMapper::AGX};
    bool bloomEnabled {};
    float bloomStrength {};
};

} // namespace bh
