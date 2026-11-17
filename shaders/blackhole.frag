#version 430 core

// Stage 0 placeholder: rays travel in STRAIGHT lines (flat space, no lensing).
// The shadow is an opaque sphere of the Schwarzschild critical impact parameter 3*sqrt(3) M
// and the disk a flat annulus from the ISCO outwards. Stage 2/3 of TODO.md replace this
// with geodesic integration in the Kerr metric.
//
// Units G = c = M = 1. World frame: spin axis +z, disk in the plane z = 0.

layout(location = 0) out vec4 outColor;

uniform vec2  uResolution;      // framebuffer size in pixels
uniform vec3  uCameraPosition;
uniform mat3  uCameraBasis;     // columns: right, up, forward
uniform float uTanHalfFov;      // tan(vertical FOV / 2)
uniform float uShadowRadius;
uniform bool  uDiskEnabled;
uniform float uDiskInnerRadius;
uniform float uDiskOuterRadius;
uniform bool  uStarsEnabled;
uniform float uExposure;
uniform float uTime;            // seconds; unused until the disk rotates

const float INFINITY = 1.0e30;
const float DISK_PEAK_TEMPERATURE = 9000.0; // K; Stage 4 derives it from the accretion rate

// "Hash without Sine", Dave Hoskins (MIT): three uniform values in [0, 1) per cell.
vec3 hash33(vec3 p)
{
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx);
}

// Approximate colour of a black body (Tanner Helland's fit, 1000-40000 K), linear RGB with
// max component 1. Stage 4 replaces it with a LUT integrated from the CIE colour matching functions.
vec3 blackbodyColor(float kelvin)
{
    float t = clamp(kelvin, 1000.0, 40000.0) / 100.0;
    float r = t <= 66.0 ? 1.0 : 1.29293618 * pow(t - 60.0, -0.1332047592);
    float g = t <= 66.0 ? 0.39008158 * log(t) - 0.63184144 : 1.12989086 * pow(t - 60.0, -0.0755148492);
    float b = t >= 66.0 ? 1.0 : (t <= 19.0 ? 0.0 : 0.54320679 * log(t - 10.0) - 1.19625409);
    return pow(clamp(vec3(r, g, b), 0.0, 1.0), vec3(2.2)); // the fit is in sRGB; return linear
}

// Procedural sky: a faint galactic band plus three layers of point stars.
vec3 starfield(vec3 direction)
{
    const vec3 galacticPole = normalize(vec3(0.35, -0.25, 1.0));
    float band = exp(-pow(dot(direction, galacticPole) / 0.2, 2.0));
    vec3 color = vec3(0.010, 0.009, 0.014) * band;

    for (int layer = 0; layer < 3; ++layer) {
        float scale = 120.0 * float(layer + 1);
        vec3 p = direction * scale;
        vec3 cell = floor(p);
        vec3 rnd = hash33(cell + 113.0 * float(layer));
        if (rnd.x < mix(0.93, 0.80, band)) {
            continue; // no star in this cell
        }
        vec3 starCenter = cell + 0.2 + 0.6 * hash33(cell.zxy + 7.0);
        float profile = smoothstep(0.3, 0.0, length(p - starCenter));
        float brightness = 4.0 * pow(rnd.y, 6.0);
        color += profile * brightness * blackbodyColor(mix(2800.0, 15000.0, rnd.z));
    }
    return color;
}

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

void main()
{
    vec2 screen = (2.0 * gl_FragCoord.xy - uResolution) / uResolution.y; // y in [-1, 1]
    vec3 direction = normalize(uCameraBasis * vec3(screen * uTanHalfFov, 1.0));
    vec3 origin = uCameraPosition;

    // Nearest hit with the shadow sphere.
    float tHole = INFINITY;
    float halfB = dot(origin, direction);
    float discriminant = halfB * halfB - (dot(origin, origin) - uShadowRadius * uShadowRadius);
    if (discriminant >= 0.0) {
        float t = -halfB - sqrt(discriminant);
        if (t > 0.0) {
            tHole = t;
        }
    }

    // Hit with the disk plane z = 0.
    float tDisk = INFINITY;
    vec3 diskColor = vec3(0.0);
    if (uDiskEnabled && abs(direction.z) > 1.0e-6) {
        float t = -origin.z / direction.z;
        float r = length((origin + t * direction).xy);
        if (t > 0.0 && r >= uDiskInnerRadius && r <= uDiskOuterRadius) {
            tDisk = t;
            diskColor = diskEmission(r);
        }
    }

    vec3 color = vec3(0.0);
    if (tDisk < tHole) {
        color = diskColor;
    } else if (tHole == INFINITY && uStarsEnabled) {
        color = starfield(direction);
    }

    color = vec3(1.0) - exp(-uExposure * color); // exposure tone mapping, HDR -> [0, 1)
    outColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
