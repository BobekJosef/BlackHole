#pragma once
// C++ mirror of the std140 uniform block `Frame` in shaders/common/frame.glsl.
// Change both together; the static_asserts pin the std140 offsets.

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>

namespace bh {

inline constexpr std::uint32_t frameFlagDisk = 1U << 0U;                  // FLAG_DISK
inline constexpr std::uint32_t frameFlagDoppler = 1U << 1U;               // FLAG_DOPPLER
inline constexpr std::uint32_t frameFlagGravitationalRedshift = 1U << 2U; // FLAG_GRAVITATIONAL_REDSHIFT
inline constexpr std::uint32_t frameFlagLimbDarkening = 1U << 3U;         // FLAG_LIMB_DARKENING

inline constexpr unsigned frameUniformBinding = 0;

struct FrameUniforms {
    glm::mat4 cameraFrame {1.0F}; // covariant ZAMO tetrad: columns u, right, up, forward
    glm::vec4 cameraPosition {};  // xyz; w unused
    glm::vec2 resolution {};      // trace resolution in pixels
    float tanHalfFov {};
    float time {};                // animation clock, coordinate time in M
    float spin {};
    float captureRadius {};
    float diskInnerRadius {};
    float diskOuterRadius {};
    float stepScale {};
    float maxStepFraction {};
    float escapeRadius {};
    std::uint32_t maxSteps {};
    std::uint32_t flags {};
    std::uint32_t debugView {};
    float diskPeakTemperature {};
    float diskBrightness {};
    float diskOrbitSign {};
    float blackbodyLogTMin {};
    float blackbodyLogTMax {};
    std::uint32_t sky {};           // SKY_*
    glm::mat4 skyRotation {1.0F};   // world -> sky map direction (upper 3x3)
    float skyBrightness {};
    float diskTurbulence {};
    float diskEddySize {};
    float diskEdgeSoftness {};
};

static_assert(offsetof(FrameUniforms, cameraFrame) == 0);
static_assert(offsetof(FrameUniforms, cameraPosition) == 64);
static_assert(offsetof(FrameUniforms, resolution) == 80);
static_assert(offsetof(FrameUniforms, tanHalfFov) == 88);
static_assert(offsetof(FrameUniforms, time) == 92);
static_assert(offsetof(FrameUniforms, spin) == 96);
static_assert(offsetof(FrameUniforms, captureRadius) == 100);
static_assert(offsetof(FrameUniforms, diskInnerRadius) == 104);
static_assert(offsetof(FrameUniforms, diskOuterRadius) == 108);
static_assert(offsetof(FrameUniforms, stepScale) == 112);
static_assert(offsetof(FrameUniforms, maxStepFraction) == 116);
static_assert(offsetof(FrameUniforms, escapeRadius) == 120);
static_assert(offsetof(FrameUniforms, maxSteps) == 124);
static_assert(offsetof(FrameUniforms, flags) == 128);
static_assert(offsetof(FrameUniforms, debugView) == 132);
static_assert(offsetof(FrameUniforms, diskPeakTemperature) == 136);
static_assert(offsetof(FrameUniforms, diskBrightness) == 140);
static_assert(offsetof(FrameUniforms, diskOrbitSign) == 144);
static_assert(offsetof(FrameUniforms, blackbodyLogTMin) == 148);
static_assert(offsetof(FrameUniforms, blackbodyLogTMax) == 152);
static_assert(offsetof(FrameUniforms, sky) == 156);
static_assert(offsetof(FrameUniforms, skyRotation) == 160);
static_assert(offsetof(FrameUniforms, skyBrightness) == 224);
static_assert(offsetof(FrameUniforms, diskTurbulence) == 228);
static_assert(offsetof(FrameUniforms, diskEddySize) == 232);
static_assert(offsetof(FrameUniforms, diskEdgeSoftness) == 236);
static_assert(sizeof(FrameUniforms) == 240);

} // namespace bh
