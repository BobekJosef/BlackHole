#ifndef SKY_GLSL
#define SKY_GLSL

#include "color.glsl"
#include "random.glsl"

// Procedural sky, NOT physical: a faint galactic band plus three layers of point stars.
// Stage 5 adds image backgrounds; this stays as the zero-dependency default.
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

#endif
