#pragma once
// Everything the renderer needs for one frame, already converted to GPU-friendly floats.

#include <glm/glm.hpp>

namespace bh {

struct FrameParams {
    glm::ivec2 resolution {};
    glm::vec3 cameraPosition {};
    glm::mat3 cameraBasis {1.0F};
    float tanHalfFov {};
    float shadowRadius {};
    bool diskEnabled {};
    float diskInnerRadius {};
    float diskOuterRadius {};
    bool starsEnabled {};
    float exposure {};
    float time {};
};

} // namespace bh
