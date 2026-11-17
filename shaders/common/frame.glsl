#ifndef FRAME_GLSL
#define FRAME_GLSL

// Per-frame data shared by the passes. The layout must match src/render/FrameUniforms.hpp,
// whose static_asserts pin the std140 offsets.
layout(std140, binding = 0) uniform Frame {
    mat4  uCameraBasis;      // columns: right, up, forward (xyz)
    vec4  uCameraPosition;   // xyz in units of M
    vec2  uResolution;       // trace resolution in pixels
    float uTanHalfFov;       // tan(vertical FOV / 2)
    float uTime;             // seconds
    float uShadowRadius;     // Stage 0 placeholder shadow sphere
    float uDiskInnerRadius;  // ISCO
    float uDiskOuterRadius;
    uint  uFlags;            // FLAG_* bits
};

const uint FLAG_DISK  = 1u << 0;
const uint FLAG_STARS = 1u << 1;

bool hasFlag(uint flag)
{
    return (uFlags & flag) != 0u;
}

#endif
