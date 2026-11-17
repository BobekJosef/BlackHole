#pragma once
// Everything the renderer needs for one frame, already converted to GPU-friendly floats.

#include <glm/glm.hpp>

namespace bh {

// The values are the TONE_MAPPER_* constants in shaders/common/color.glsl.
enum class ToneMapper { AGX = 0, ACES = 1, EXPONENTIAL = 2, CLAMP = 3 };

struct FrameParams {
    // Output and resolution
    glm::ivec2 outputSize {};  // window framebuffer in pixels
    float renderScale {1.0F};  // trace resolution / output resolution

    // Scene
    glm::vec3 cameraPosition {};
    glm::mat3 cameraBasis {1.0F};
    float tanHalfFov {};
    float shadowRadius {};
    bool diskEnabled {};
    float diskInnerRadius {};
    float diskOuterRadius {};
    bool starsEnabled {};
    float time {};

    // Post-processing
    float exposure {1.0F}; // linear factor
    ToneMapper toneMapper {ToneMapper::AGX};
    bool bloomEnabled {};
    float bloomStrength {};
};

} // namespace bh
