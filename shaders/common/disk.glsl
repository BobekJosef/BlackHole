#ifndef DISK_GLSL
#define DISK_GLSL

#include "frame.glsl"

// Novikov-Thorne thin disk: gas on circular geodesics, emitting as a black body at the local
// temperature T(r) = uDiskPeakTemperature * profile(r), profile = (F / F_max)^(1/4) from the
// Page-Thorne flux (src/physics/Disk.cpp, sampled into uDiskProfile).
//
// Seen through redshift g = nu_obs / nu_em, a black body at T looks exactly like a black body at g T
// (I_nu / nu^3 is invariant), so colour and the g^4 beaming both come from one look-up at g T.

layout(binding = 2) uniform sampler1D uDiskProfile; // over u = sqrt((r - r_in) / (r_out - r_in))
layout(binding = 3) uniform sampler1D uBlackbody;   // linear sRGB radiance over log10 T, Y(6500 K) = 1

// Texture coordinate of the centre of the texel nearest to u in [0, 1].
float lutCoordinate(sampler1D lut, float u)
{
    float size = float(textureSize(lut, 0));
    return (clamp(u, 0.0, 1.0) * (size - 1.0) + 0.5) / size;
}

vec3 blackbodyRadiance(float kelvin)
{
    float u = (log(max(kelvin, 1.0)) / log(10.0) - uBlackbodyLogTMin) / (uBlackbodyLogTMax - uBlackbodyLogTMin);
    return u < 0.0 ? vec3(0.0) : texture(uBlackbody, lutCoordinate(uBlackbody, u)).rgb;
}

// Redshift g = nu_obs / nu_em of light from the disk gas at Boyer-Lindquist radius r (equator), for a ray
// with conserved p_t and L_z (camera ZAMO measures frequency 1). Float copy of kerr::diskRedshift in
// src/physics/Disk.cpp, which documents the formulas and is tested against explicit 4-vector
// contractions: gravitational (incl. frame dragging) via the emitter's ZAMO, times Doppler.
float diskRedshift(float r, float pt, float angularMomentum)
{
    float a = uSpin;
    float omega = uDiskOrbitSign / (r * sqrt(r) + uDiskOrbitSign * a);
    float r2a2 = r * r + a * a;
    float oneMinusAOmega = 1.0 - a * omega;
    float ut = inversesqrt(1.0 - omega * omega * r2a2 - 2.0 / r * oneMinusAOmega * oneMinusAOmega);
    float nuEmitter = ut * (pt + omega * angularMomentum);

    float delta = r * r - 2.0 * r + a * a;
    float bigA = r2a2 * r2a2 - a * a * delta;
    float lapse = r * sqrt(delta / bigA);
    float frameDragging = 2.0 * a * r / bigA;
    float nuZamo = (pt + frameDragging * angularMomentum) / lapse;

    float g = 1.0;
    if (hasFlag(FLAG_GRAVITATIONAL_REDSHIFT)) {
        g /= nuZamo;
    }
    if (hasFlag(FLAG_DOPPLER)) {
        g *= nuZamo / nuEmitter;
    }
    return g;
}

// Observed radiance of the disk at Boyer-Lindquist radius r, seen with redshift g.
vec3 diskRadiance(float r, float g)
{
    float u = sqrt((r - uDiskInnerRadius) / (uDiskOuterRadius - uDiskInnerRadius));
    float temperature = uDiskPeakTemperature * texture(uDiskProfile, lutCoordinate(uDiskProfile, u)).r;
    return uDiskBrightness * blackbodyRadiance(g * temperature);
}

#endif
