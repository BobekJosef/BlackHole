#ifndef RANDOM_GLSL
#define RANDOM_GLSL

// "Hash without Sine", Dave Hoskins (MIT): three uniform values in [0, 1) per input point.
vec3 hash33(vec3 p)
{
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx);
}

#endif
