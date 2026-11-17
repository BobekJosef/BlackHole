#ifndef COLOR_GLSL
#define COLOR_GLSL

// Colour science helpers. All radiance is linear Rec.709/sRGB primaries until linearToSrgb.

// Must match bh::ToneMapper in src/render/FrameParams.hpp.
const int TONE_MAPPER_AGX = 0;
const int TONE_MAPPER_ACES = 1;
const int TONE_MAPPER_EXPONENTIAL = 2;
const int TONE_MAPPER_CLAMP = 3;

float luminance(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722)); // Rec.709
}

// ACES filmic curve, Stephen Hill's fit of the RRT + sRGB ODT (BakingLab, MIT).
vec3 toneMapAces(vec3 color)
{
    const mat3 inputMatrix = mat3(0.59719, 0.07600, 0.02840,   // sRGB -> AP1 (with RRT saturation)
                                  0.35458, 0.90834, 0.13383,
                                  0.04823, 0.01566, 0.83777);
    const mat3 outputMatrix = mat3(1.60475, -0.10208, -0.00327, // ODT saturation, AP1 -> sRGB
                                   -0.53108, 1.10813, -0.07276,
                                   -0.07367, -0.00605, 1.07602);
    vec3 v = inputMatrix * color;
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return clamp(outputMatrix * (a / b), 0.0, 1.0);
}

// AgX (Troy Sobotka), "minimal AgX" polynomial fit by Benjamin Wrensch. Desaturates very bright
// highlights towards white instead of clipping hue: suits a blackbody-hot disk.
vec3 toneMapAgx(vec3 color)
{
    const mat3 inset = mat3(0.842479062253094, 0.0423282422610123, 0.0423756549057051,
                            0.0784335999999992, 0.878468636469772, 0.0784336,
                            0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    const mat3 outset = mat3(1.19687900512017, -0.0528968517574562, -0.0529716355144438,
                             -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
                             -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
    const float minEv = -12.47393;
    const float maxEv = 4.026069;

    vec3 x = inset * max(color, vec3(1.0e-10));
    x = (clamp(log2(x), minEv, maxEv) - minEv) / (maxEv - minEv);
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;
    x = 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
    x = outset * x;
    return pow(clamp(x, 0.0, 1.0), vec3(2.2)); // the curve outputs display-encoded values; linearise
}

// HDR linear -> display linear in [0, 1].
vec3 toneMap(vec3 color, int toneMapper)
{
    switch (toneMapper) {
    case TONE_MAPPER_AGX: return toneMapAgx(color);
    case TONE_MAPPER_ACES: return toneMapAces(color);
    case TONE_MAPPER_EXPONENTIAL: return vec3(1.0) - exp(-color);
    default: return clamp(color, 0.0, 1.0);
    }
}

// sRGB OETF (IEC 61966-2-1).
vec3 linearToSrgb(vec3 color)
{
    vec3 low = 12.92 * color;
    vec3 high = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
    return mix(high, low, lessThanEqual(color, vec3(0.0031308)));
}

#endif
