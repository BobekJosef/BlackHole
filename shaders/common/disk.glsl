#ifndef DISK_GLSL
#define DISK_GLSL

#include "color.glsl"
#include "frame.glsl"

const float DISK_PEAK_TEMPERATURE = 9000.0; // K; Stage 4 derives it from the accretion rate

// Thin-disk emission with the Newtonian flux profile F ~ r^-3 (1 - sqrt(r_in / r)),
// which peaks at r = 49/36 r_in. Local temperature T ~ F^(1/4).
// Stage 4: Page-Thorne flux, Doppler and gravitational redshift g, I_obs = g^4 I_em.
vec3 diskEmission(float r)
{
    float rPeak = 49.0 / 36.0 * uDiskInnerRadius;
    float flux = (1.0 - sqrt(uDiskInnerRadius / r)) / (r * r * r);
    float fluxPeak = (1.0 - sqrt(uDiskInnerRadius / rPeak)) / (rPeak * rPeak * rPeak);
    float relativeFlux = flux / fluxPeak;
    float temperature = DISK_PEAK_TEMPERATURE * pow(relativeFlux, 0.25);
    float outerFade = smoothstep(uDiskOuterRadius, 0.8 * uDiskOuterRadius, r);
    return 1.5 * relativeFlux * outerFade * blackbodyColor(temperature);
}

#endif
