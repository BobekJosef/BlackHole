#ifndef SKY_GLSL
#define SKY_GLSL

#include "color.glsl"
#include "disk.glsl"
#include "frame.glsl"
#include "random.glsl"

// The background, seen along the direction an escaped ray finally travels in (the light came from there).
//
// Filtering. Lensing magnifies the sky enormously along the critical curve and squeezes it next to the
// shadow, so what one pixel sees of the sky has nothing to do with its size on screen (and a compute
// shader has no derivatives anyway). trace.comp passes a ray differential instead: the change of the
// escape direction to the next pixel in x and in y, taken from the neighbouring invocations. Every
// background filters with that footprint: image maps through textureGrad (mipmaps + anisotropic
// filtering), grid lines and stars analytically. The same footprint gives the lensing magnification
// mu = (pixel solid angle) / (sky solid angle the pixel sees), which the stars need.
//
// The sky map direction is uSkyRotation * (world direction): yaw about the spin axis, then tilt.

layout(binding = 4) uniform sampler2D uSkyEquirectangular;
layout(binding = 5) uniform samplerCube uSkyCube;

const float PI = 3.14159265358979;

// Black body at `kelvin` seen with shift g, which is a black body at g T, normalised to luminance 1 at g = 1.
vec3 shiftedBlackbody(float kelvin, float g)
{
    return blackbodyRadiance(g * kelvin) / luminance(blackbodyRadiance(kelvin));
}

// ---- Procedural stars ----------------------------------------------------------------------------
//
// NOT a catalogue: positions, fluxes and temperatures are random. Fluxes follow the Euclidean number
// counts N(> F) ~ F^-3/2 of a uniform population, between minFlux and STAR_FLUX_RANGE x minFlux.
// What is physical is how they are seen:
//  - a star is a point source. The pixel it falls in collects its flux F times the magnification mu,
//    i.e. radiance F / (sky solid angle of the pixel), whatever the magnification;
//  - its spectrum is a black body at T seen with the camera's shift g, i.e. a black body at g T.
// Each star is splatted as a Gaussian STAR_SIGMA pixels wide in screen space, found through the
// inverse of the footprint Jacobian; that conserves flux at any magnification and keeps stars one
// pixel small even where lensing stretches the sky. Where the footprint grows past the spacing of a
// layer's stars (next to the shadow) they can no longer be resolved one by one, and the layer fades
// into its mean radiance, as a mipmap would.

const float STAR_SIGMA = 0.5;             // pixels
const float STAR_FLUX_RANGE = 100.0;      // brightest / faintest star of a layer
const float STAR_MEAN_FLUX = 2.8;         // <F> / minFlux for that range: 3 - 2 STAR_FLUX_RANGE^(-1/2)
const float STAR_MEAN_TEMPERATURE = 5000.0;

// Stars live in the unit cells of a cubic lattice scaled by `scale`: a cell holds one with probability
// `probability`, at a random point P, kept only in the shell scale - 1/2 < |P| < scale + 1/2. That makes
// them uniform on the sky, probability x scale^2 per steradian, and only the few cells within about
// half a lattice unit of d x scale can hold a star closer to d than 1/(2 scale) rad, the search radius.
vec3 starLayer(vec3 d, vec3 jx, vec3 jy, float area, float spread, float scale, float probability,
               float minFlux, float seed, float g)
{
    float searchRadius = 0.5 / scale;
    vec3 mean = probability * scale * scale * STAR_MEAN_FLUX * minFlux * shiftedBlackbody(STAR_MEAN_TEMPERATURE, g);
    float unresolved = smoothstep(0.6, 1.0, 2.5 * STAR_SIGMA * spread / searchRadius);
    if (unresolved >= 1.0) {
        return mean;
    }

    // Screen-space offset of a sky offset v: the least-squares solution of J delta = v, J = (jx jy), i.e.
    // (J^T J)^-1 J^T v. On the critical curve the footprint collapses onto a line and J^T J is singular:
    // a point source would be infinitely magnified. A real star has a finite size, which caps that; the
    // regularisation J^T J + epsilon^2 I stands for it, with epsilon = 1e-3 pixel (mu <~ 1000, the cap
    // `area` has too). The determinant is |jx x jy|^2 (Lagrange's identity): exact and never negative,
    // whereas ac - b^2 cancels catastrophically in float (it went negative there and made NaNs, which the
    // bloom spread into black squares).
    float a = dot(jx, jx);
    float b = dot(jx, jy);
    float c = dot(jy, jy);
    float pixelAngle = 2.0 * uTanHalfFov / uResolution.y;
    float epsilon2 = 1.0e-6 * pixelAngle * pixelAngle;
    vec3 areaVector = cross(jx, jy);
    float det = dot(areaVector, areaVector) + epsilon2 * (a + c + epsilon2);
    mat2 toPixels = mat2(c + epsilon2, -b, -b, a + epsilon2) / det;

    vec3 q = d * scale;
    vec3 extent = 0.5 * (abs(d) + sqrt(max(1.0 - d * d, 0.0))); // box around radial +-1/2, lateral 1/2
    ivec3 lo = ivec3(floor(q - extent));
    ivec3 hi = ivec3(floor(q + extent));
    vec3 sum = vec3(0.0);
    for (int x = lo.x; x <= hi.x; ++x) {
        for (int y = lo.y; y <= hi.y; ++y) {
            for (int z = lo.z; z <= hi.z; ++z) {
                vec3 cell = vec3(x, y, z);
                vec3 rnd = hash33(cell + seed);
                if (rnd.x >= probability) {
                    continue;
                }
                vec3 position = cell + hash33(cell.zxy + seed + 19.19);
                float radius = length(position);
                vec3 offset = position / radius - d;
                if (abs(radius - scale) >= 0.5 || dot(offset, offset) >= searchRadius * searchRadius) {
                    continue;
                }
                vec2 pixels = toPixels * vec2(dot(jx, offset), dot(jy, offset));
                float kernel = exp(-0.5 * dot(pixels, pixels) / (STAR_SIGMA * STAR_SIGMA));
                float flux = minFlux * pow(max(rnd.y, pow(STAR_FLUX_RANGE, -1.5)), -2.0 / 3.0);
                float temperature = mix(3000.0, 30000.0, rnd.z * rnd.z * rnd.z * rnd.z);
                sum += flux * kernel * shiftedBlackbody(temperature, g);
            }
        }
    }
    vec3 stars = sum / (2.0 * PI * STAR_SIGMA * STAR_SIGMA * area);
    return mix(stars, mean, unresolved);
}

// Faint diffuse band (unresolved stars; NOT physical) plus two layers of point stars: a sparse coarse
// layer of bright stars, which stays resolved where lensing squeezes the sky (next to the shadow), and a
// dense layer of faint ones, which fades into its mean there first, as on a real photograph.
vec3 starfield(vec3 d, vec3 jx, vec3 jy, float area, float g)
{
    // Largest stretch of the footprint (rad per pixel): the square root of the larger eigenvalue of J^T J.
    float a = dot(jx, jx);
    float b = dot(jx, jy);
    float c = dot(jy, jy);
    float spread = sqrt(0.5 * (a + c) + sqrt(0.25 * (a - c) * (a - c) + b * b));

    const vec3 galacticPole = normalize(vec3(0.35, -0.25, 1.0));
    float band = exp(-pow(dot(d, galacticPole) / 0.2, 2.0));
    vec3 radiance = 0.004 * band * shiftedBlackbody(4500.0, g);
    radiance += starLayer(d, jx, jy, area, spread, 40.0, 0.15, 2.0e-6, 0.0, g);   // ~3000 stars
    radiance += starLayer(d, jx, jy, area, spread, 110.0, 0.15, 2.5e-7, 71.0, g); // ~23000 stars
    return radiance;
}

// ---- Coordinate grid -----------------------------------------------------------------------------
//
// Lines of constant theta and phi every 10 degrees: makes the lensing map readable (the Einstein ring,
// the mirrored secondary image, frame dragging twisting the meridians). Equator red, phi = 0 green.
// NOT physical, so its colours are fixed; it gets the bolometric g^4 like an image map.

// Box-filtered coverage of lines of half-width w at x = k spacing, for a pixel spanning `footprint` in x.
float lineCoverage(float x, float spacing, float halfWidth, float footprint)
{
    footprint = max(footprint, 1.0e-7);
    float distance = x - spacing * round(x / spacing);
    float lo = max(distance - 0.5 * footprint, -halfWidth);
    float hi = min(distance + 0.5 * footprint, halfWidth);
    float nearest = max(hi - lo, 0.0) / footprint;
    // Once the footprint spans several lines, the box holds footprint / spacing of them: their mean.
    return mix(nearest, 2.0 * halfWidth / spacing, smoothstep(0.5 * spacing, spacing, footprint));
}

vec3 coordinateGrid(vec3 d, vec3 jx, vec3 jy)
{
    const float SPACING = PI / 18.0;
    const float HALF_WIDTH = 0.0015; // rad on the sky
    float theta = acos(clamp(d.z, -1.0, 1.0));
    float phi = atan(d.y, d.x);
    float rho2 = max(dot(d.xy, d.xy), 1.0e-12);
    float sinTheta = sqrt(rho2);
    // d theta = -dz / sin(theta), d phi = (x dy - y dx) / rho^2; the footprint is the box spanned by both pixel steps.
    float thetaFootprint = (abs(jx.z) + abs(jy.z)) / sinTheta;
    float phiFootprint = (abs(d.x * jx.y - d.y * jx.x) + abs(d.x * jy.y - d.y * jy.x)) / rho2;
    float phiHalfWidth = HALF_WIDTH / max(sinTheta, 1.0e-3); // constant width on the sky

    int quadrant = int(floor(phi / (0.5 * PI) + 4.0)) & 3;
    const vec3 TINTS[4] = vec3[4](vec3(0.05, 0.03, 0.02), vec3(0.02, 0.04, 0.05), vec3(0.04, 0.02, 0.05),
                                  vec3(0.02, 0.05, 0.03));
    vec3 color = TINTS[quadrant] * (d.z > 0.0 ? 1.0 : 0.5);
    float lines = max(lineCoverage(theta, SPACING, HALF_WIDTH, thetaFootprint),
                      lineCoverage(phi, SPACING, phiHalfWidth, phiFootprint));
    color = mix(color, vec3(0.35), lines);
    color = mix(color, vec3(0.6, 0.08, 0.05), lineCoverage(theta - 0.5 * PI, PI, 2.0 * HALF_WIDTH, thetaFootprint));
    color = mix(color, vec3(0.08, 0.5, 0.1), lineCoverage(phi, 2.0 * PI, 2.0 * phiHalfWidth, phiFootprint));
    return color;
}

// ---- Image maps ----------------------------------------------------------------------------------

// Equirectangular map, z up: theta from the top row down, phi decreasing to the right, so the map is
// seen unmirrored from inside; phi = 0 (+x) is the image centre.
vec3 equirectangularMap(vec3 d, vec3 jx, vec3 jy)
{
    vec2 uv = vec2(0.5 - atan(d.y, d.x) / (2.0 * PI), acos(clamp(d.z, -1.0, 1.0)) / PI);
    float rho2 = max(dot(d.xy, d.xy), 1.0e-12);
    float sinTheta = sqrt(rho2);
    // Analytic derivatives: no seam at phi = +-pi, unlike differences of uv.
    vec2 dx = vec2(-(d.x * jx.y - d.y * jx.x) / (2.0 * PI * rho2), -jx.z / (PI * sinTheta));
    vec2 dy = vec2(-(d.x * jy.y - d.y * jy.x) / (2.0 * PI * rho2), -jy.z / (PI * sinTheta));
    return textureGrad(uSkyEquirectangular, uv, dx, dy).rgb;
}

// Cube maps follow OpenGL's (originally RenderMan's) convention: y up, and the faces read correctly
// from inside a LEFT-handed frame (looking along +x, +z is to the left in the px image). The world is
// right-handed with z up, so the map is a reflection, z -> y and y -> z; a rotation would mirror every
// face (checked with labelled faces).
vec3 cubeMapDirection(vec3 v)
{
    return v.xzy;
}

vec3 cubeMap(vec3 d, vec3 jx, vec3 jy)
{
    return textureGrad(uSkyCube, cubeMapDirection(d), cubeMapDirection(jx), cubeMapDirection(jy)).rgb;
}

// ---- Entry point ---------------------------------------------------------------------------------

// Radiance of the background along unit `direction`, whose change to the next pixel in x and y is
// jx and jy; `area` is the sky solid angle of the pixel, g the camera's shift (1 / p_t).
vec3 skyRadiance(vec3 direction, vec3 jx, vec3 jy, float area, float g)
{
    if (uSky == SKY_NONE) {
        return vec3(0.0);
    }
    mat3 rotation = mat3(uSkyRotation);
    vec3 d = rotation * direction;
    jx = rotation * jx;
    jy = rotation * jy;

    vec3 radiance;
    if (uSky == SKY_STARS) {
        radiance = starfield(d, jx, jy, area, g);
    } else {
        if (uSky == SKY_GRID) {
            radiance = coordinateGrid(d, jx, jy);
        } else if (uSky == SKY_EQUIRECTANGULAR) {
            radiance = equirectangularMap(d, jx, jy);
        } else {
            radiance = cubeMap(d, jx, jy);
        }
        // An RGB image has no spectrum, so its colours cannot be shifted; only its bolometric radiance
        // can: integrating the invariant I_nu / nu^3 over frequency gives I = g^4 I_emitted. Approximate.
        radiance *= (g * g) * (g * g);
    }
    return uSkyBrightness * radiance;
}

#endif
