#ifndef FRAME_GLSL
#define FRAME_GLSL

// Per-frame data shared by the passes. The layout must match src/render/FrameUniforms.hpp,
// whose static_asserts pin the std140 offsets.
layout(std140, binding = 0) uniform Frame {
    // Camera frame (ZAMO tetrad) as covariant 4-vectors (t, x, y, z) in Kerr-Schild coordinates:
    // columns u_mu, right_mu, up_mu, forward_mu. A pixel looking along unit n (right, up, forward)
    // starts the backward ray p_mu = uCameraFrame * vec4(-1, n).
    mat4  uCameraFrame;
    vec4  uCameraPosition;   // xyz, Kerr-Schild, units of M
    vec2  uResolution;       // trace resolution in pixels
    float uTanHalfFov;       // tan(vertical FOV / 2)
    float uTime;             // seconds
    float uSpin;             // a = J/M
    float uCaptureRadius;    // Boyer-Lindquist r below which a ray counts as captured (just outside r+)
    float uDiskInnerRadius;  // ISCO
    float uDiskOuterRadius;
    float uStepScale;        // affine step h = r min(uStepScale sqrt(r), uMaxStepFraction)
    float uMaxStepFraction;
    float uEscapeRadius;     // Boyer-Lindquist r beyond which a ray samples the sky
    uint  uMaxSteps;
    uint  uFlags;            // FLAG_* bits
    uint  uDebugView;        // DEBUG_VIEW_*
    float uDiskPeakTemperature; // K, maximum of the local (emitted) temperature
    float uDiskBrightness;      // 1 / Y(peak temperature): unshifted peak has luminance 1 at 0 EV
    float uDiskOrbitSign;       // +1 prograde, -1 retrograde gas
    float uBlackbodyLogTMin;    // log10 T range of uBlackbody
    float uBlackbodyLogTMax;
};

const uint FLAG_DISK  = 1u << 0;
const uint FLAG_STARS = 1u << 1;
const uint FLAG_DOPPLER = 1u << 2;
const uint FLAG_GRAVITATIONAL_REDSHIFT = 1u << 3;

// Must match bh::DebugView in src/render/FrameParams.hpp.
const uint DEBUG_VIEW_NONE = 0u;
const uint DEBUG_VIEW_STEPS = 1u;
const uint DEBUG_VIEW_CONSTRAINT = 2u;
const uint DEBUG_VIEW_FATE = 3u;
const uint DEBUG_VIEW_REDSHIFT = 4u;

bool hasFlag(uint flag)
{
    return (uFlags & flag) != 0u;
}

#endif
