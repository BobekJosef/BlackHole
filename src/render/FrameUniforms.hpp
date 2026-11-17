#pragma once
// C++ mirror of the std140 uniform block `Frame` in shaders/common/frame.glsl.
// Change both together; the static_asserts pin the std140 offsets.

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>

namespace bh {

inline constexpr std::uint32_t frameFlagDisk = 1U << 0U;  // FLAG_DISK
inline constexpr std::uint32_t frameFlagStars = 1U << 1U; // FLAG_STARS

inline constexpr unsigned frameUniformBinding = 0;

struct FrameUniforms {
    glm::mat4 cameraBasis {1.0F};  // columns right, up, forward; mat4 because std140 pads mat3 columns
    glm::vec4 cameraPosition {};   // xyz; w unused
    glm::vec2 resolution {};       // trace resolution in pixels
    float tanHalfFov {};
    float time {};
    float shadowRadius {};
    float diskInnerRadius {};
    float diskOuterRadius {};
    std::uint32_t flags {};
};

static_assert(offsetof(FrameUniforms, cameraBasis) == 0);
static_assert(offsetof(FrameUniforms, cameraPosition) == 64);
static_assert(offsetof(FrameUniforms, resolution) == 80);
static_assert(offsetof(FrameUniforms, tanHalfFov) == 88);
static_assert(offsetof(FrameUniforms, time) == 92);
static_assert(offsetof(FrameUniforms, shadowRadius) == 96);
static_assert(offsetof(FrameUniforms, diskInnerRadius) == 100);
static_assert(offsetof(FrameUniforms, diskOuterRadius) == 104);
static_assert(offsetof(FrameUniforms, flags) == 108);
static_assert(sizeof(FrameUniforms) == 112);

} // namespace bh
