#ifndef DISK_GLSL
#define DISK_GLSL

#include "frame.glsl"
#include "random.glsl"

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

// Keplerian angular velocity d(phi)/dt of the gas (Bardeen, Press & Teukolsky 1972), signed by the orbit.
float keplerianOmega(float r)
{
    return uDiskOrbitSign / (r * sqrt(r) + uDiskOrbitSign * uSpin);
}

// Frequency of the ray measured by the gas, nu_em = u^t (p_t + Omega L_z) (camera measures 1).
float diskEmitterFrequency(float r, float pt, float angularMomentum)
{
    float a = uSpin;
    float omega = keplerianOmega(r);
    float r2a2 = r * r + a * a;
    float oneMinusAOmega = 1.0 - a * omega;
    float ut = inversesqrt(1.0 - omega * omega * r2a2 - 2.0 / r * oneMinusAOmega * oneMinusAOmega);
    return ut * (pt + omega * angularMomentum);
}

// Redshift g = nu_obs / nu_em of light from the disk gas at Boyer-Lindquist radius r (equator), for a ray
// with conserved p_t and L_z (camera ZAMO measures frequency 1). Float copy of kerr::diskRedshift in
// src/physics/Disk.cpp, which documents the formulas and is tested against explicit 4-vector
// contractions: gravitational (incl. frame dragging) via the emitter's ZAMO, times Doppler.
float diskRedshift(float r, float pt, float angularMomentum)
{
    float a = uSpin;
    float r2a2 = r * r + a * a;
    float nuEmitter = diskEmitterFrequency(r, pt, angularMomentum);

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

// Cosine of the emission angle from the disk normal in the gas frame, mu = |p_z| / nu_em: float copy of
// kerr::diskEmissionCosine (tested against a projection onto the normal). Always the true g, whatever
// the Doppler / gravitational toggles say.
float diskEmissionCosine(float r, float pt, float angularMomentum, float pz)
{
    return min(abs(pz) / diskEmitterFrequency(r, pt, angularMomentum), 1.0);
}

// Opacity of a disk crossing at radius r seen at emission cosine mu: float copy of kerr::diskEdgeOpacity
// (the outer disk thins out over the last uDiskEdgeSoftness of its radius; slab transfer exp(-tau / mu)).
float diskOpacity(float r, float mu)
{
    float start = uDiskOuterRadius * (1.0 - uDiskEdgeSoftness);
    if (uDiskEdgeSoftness <= 0.0 || r <= start) {
        return 1.0;
    }
    float taper = 1.0 - smoothstep(start, uDiskOuterRadius, r);
    return 1.0 - pow(1.0 - taper, 1.0 / max(mu, 1.0e-3));
}

// Electron-scattering limb darkening, flux-conserving: kerr::electronScatteringLimbDarkening.
float limbDarkening(float mu)
{
    return (1.0 + 2.06 * mu) / (1.0 + 2.0 * 2.06 / 3.0);
}

// ---- Turbulence: a statistical model, NOT a simulation ------------------------------------------
//
// Real thin disks are turbulent (the magnetorotational instability, Balbus & Hawley 1991); the smooth
// Novikov-Thorne disk is their average over time and azimuth. The model multiplies the local flux by a
// lognormal random field, F = F_NT exp(sigma n - sigma^2 Var(n) / 2), the distribution MRI simulations find
// for the surface density; for Gaussian n that keeps the mean flux exactly F_NT. Its structure follows
// the physics of the flow:
//  - eddies are as large as the disk is thick: radial size H = h r, azimuthal A h r (MRI turbulence is
//    stretched along the flow);
//  - they move with the gas at Omega(r), so the Keplerian shear (d ln Omega / d ln r = -3/2) winds them
//    into trailing spirals;
//  - they live about one local orbit (the MRI correlation time): two generations of noise crossfade,
//    each advected from its birth, so the shear (and the stretching of the eddies) stays bounded.
// Lifetimes come in octave bands, T_b = 2^b M, picked by the local orbital period; all radii of a band
// share one generation clock t / T_b, so neighbouring radii always carry patterns of the same age.
// (A lifetime proportional to the local period, phase t / T(r), has a radial phase gradient that grows
// with t: after some thousand M the generations interleave below the eddy size and the pattern
// shatters into noise and spokes.) Adjacent bands are blended in log T, so no band edge shows.
// Not physical: the noise itself and its spectrum (amplitude 2^(-1/3) per octave, Kolmogorov-like).
// Eddies smaller than the pixel's footprint on the disk fade out (they would alias), leaving the mean:
// a distant disk looks smooth, as it would.

const float EDDY_ASPECT = 4.0;          // azimuthal / radial eddy size
const float EDDY_LIFETIME_ORBITS = 1.0;
const int TURBULENCE_OCTAVES = 4;
const float GRADIENT_NOISE_VARIANCE = 0.0364; // of gradientNoise over space (measured, 2e5 samples)
// A rotation between octaves, so their lattices do not line up (which shows as axis-aligned streaks).
const mat3 OCTAVE_ROTATION = mat3(0.00, 0.80, 0.60, -0.80, 0.36, -0.48, -0.60, -0.48, 0.64);

float noiseCorner(vec3 cell, vec3 f, vec3 offset)
{
    // Unit-length gradients: random ones from a cube favour the diagonals.
    vec3 gradient = hash33(cell + offset) * 2.0 - 1.0;
    return dot(gradient * inversesqrt(dot(gradient, gradient) + 1.0e-6), f - offset);
}

// Perlin gradient noise with quintic interpolation: zero mean, variance GRADIENT_NOISE_VARIANCE.
float gradientNoise(vec3 p)
{
    vec3 cell = floor(p);
    vec3 f = p - cell;
    vec3 w = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float x00 = mix(noiseCorner(cell, f, vec3(0, 0, 0)), noiseCorner(cell, f, vec3(1, 0, 0)), w.x);
    float x10 = mix(noiseCorner(cell, f, vec3(0, 1, 0)), noiseCorner(cell, f, vec3(1, 1, 0)), w.x);
    float x01 = mix(noiseCorner(cell, f, vec3(0, 0, 1)), noiseCorner(cell, f, vec3(1, 0, 1)), w.x);
    float x11 = mix(noiseCorner(cell, f, vec3(0, 1, 1)), noiseCorner(cell, f, vec3(1, 1, 1)), w.x);
    return mix(mix(x00, x10, w.y), mix(x01, x11, w.y), w.z);
}

// Turbulent field of one lifetime band (lifetime 2^band M): two generations half a lifetime apart with
// triangle weights, renormalised so the variance does not dip during the crossfade. Unit variance when
// every octave is resolved.
float turbulenceBand(float band, float radial, float ring, float phi, float omega,
                     float weights[TURBULENCE_OCTAVES], float totalVariance)
{
    float lifetime = exp2(band);
    float n = 0.0;
    float blendNorm = 0.0;
    for (int generation = 0; generation < 2; ++generation) {
        float phase = uTime / lifetime + 0.5 * float(generation);
        float index = floor(phase);
        float age = (phase - index) * lifetime;
        float weight = 1.0 - abs(2.0 * (phase - index) - 1.0);
        vec3 seed = (hash33(vec3(index, float(generation) + 0.37, band)) - 0.5) * 4096.0;
        float advected = phi - omega * age; // the pattern moves with the gas: sheared by Omega(r)
        vec3 q = vec3(radial, ring * cos(advected), ring * sin(advected));
        float field = 0.0;
        for (int octave = 0; octave < TURBULENCE_OCTAVES; ++octave) {
            if (weights[octave] <= 0.0) {
                break; // the weights only fall with the octave: the rest are below the footprint too
            }
            field += weights[octave] * gradientNoise(q + seed);
            q = OCTAVE_ROTATION * q * 2.0;
        }
        n += weight * field;
        blendNorm += weight * weight;
    }
    return n / sqrt(blendNorm * totalVariance * GRADIENT_NOISE_VARIANCE);
}

// Flux factor F / F_NT at Boyer-Lindquist radius r and azimuth phi at coordinate time uTime (M), for a
// pixel whose footprint on the disk is `footprint` M across. 1 when turbulence is off.
float diskTurbulence(float r, float phi, float footprint)
{
    float sigma = uDiskTurbulence;
    float h = uDiskEddySize;
    if (sigma <= 0.0) {
        return 1.0;
    }
    float omega = keplerianOmega(r);
    float radial = log(r) / h;              // one unit = one eddy, H = h r
    float ring = 1.0 / (EDDY_ASPECT * h);   // radius of the noise circle: one unit = A h r along the orbit

    // Octave weights: Kolmogorov-like amplitudes, faded once an eddy is smaller than ~3 footprints.
    float weights[TURBULENCE_OCTAVES];
    float totalVariance = 0.0;
    float keptVariance = 0.0;
    for (int octave = 0; octave < TURBULENCE_OCTAVES; ++octave) {
        float amplitude = exp2(-float(octave) / 3.0);
        float eddy = h * r * exp2(-float(octave));
        weights[octave] = amplitude * (1.0 - smoothstep(0.15, 0.35, footprint / eddy));
        totalVariance += amplitude * amplitude;
        keptVariance += weights[octave] * weights[octave];
    }
    if (keptVariance <= 0.0) {
        return 1.0;
    }

    // The two lifetime bands around the local one, blended in log T (independent fields: renormalised).
    float logLifetime = log2(EDDY_LIFETIME_ORBITS * 2.0 * 3.14159265 / abs(omega));
    float band = floor(logLifetime);
    float w = logLifetime - band;
    float n = (1.0 - w) * turbulenceBand(band, radial, ring, phi, omega, weights, totalVariance)
            + w * turbulenceBand(band + 1.0, radial, ring, phi, omega, weights, totalVariance);
    n /= sqrt((1.0 - w) * (1.0 - w) + w * w);

    // Unit variance when every octave is resolved; less (smoother) when the fine ones fade.
    float variance = keptVariance / totalVariance;
    return exp(sigma * n - 0.5 * sigma * sigma * variance);
}

// Observed radiance emitted by one crossing of the disk at Boyer-Lindquist radius r, seen with redshift g
// at emission cosine mu; fluxFactor = F / F_NT from the turbulence (T scales as F^(1/4)). A layer of
// opacity alpha emits alpha B (slab transfer, source function B); limb darkening belongs to the
// optically thick atmosphere, so it fades in with alpha.
vec3 diskRadiance(float r, float g, float mu, float fluxFactor, float opacity)
{
    float u = sqrt((r - uDiskInnerRadius) / (uDiskOuterRadius - uDiskInnerRadius));
    float temperature = uDiskPeakTemperature * texture(uDiskProfile, lutCoordinate(uDiskProfile, u)).r
                      * sqrt(sqrt(fluxFactor));
    vec3 radiance = uDiskBrightness * blackbodyRadiance(g * temperature);
    float limb = hasFlag(FLAG_LIMB_DARKENING) ? mix(1.0, limbDarkening(mu), opacity) : 1.0;
    return opacity * limb * radiance;
}

#endif
