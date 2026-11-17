#version 430 core
#extension GL_GOOGLE_include_directive : enable

// Final pass: up-scales the traced HDR image to the window, mixes in bloom, applies exposure
// and tone mapping, and encodes to sRGB.

#include "common/color.glsl"
#include "common/random.glsl"

layout(binding = 0) uniform sampler2D uRadiance;
layout(binding = 1) uniform sampler2D uBloom;

uniform vec2  uOutputSize;      // window framebuffer size in pixels
uniform float uExposure;        // linear factor 2^EV
uniform int   uToneMapper;      // TONE_MAPPER_*
uniform bool  uBloomEnabled;
uniform float uBloomStrength;   // fraction of energy moved into the blur, 0..1
uniform float uBloomNormalization; // 1 / number of bloom levels summed by the up-sampling chain

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 uv = gl_FragCoord.xy / uOutputSize;
    vec3 color = texture(uRadiance, uv).rgb;
    if (uBloomEnabled) {
        // Energy-conserving: a fraction of the light is spread out, none is added.
        color = mix(color, uBloomNormalization * texture(uBloom, uv).rgb, uBloomStrength);
    }
    vec3 encoded = linearToSrgb(toneMap(uExposure * color, uToneMapper));
    encoded += (hash33(vec3(gl_FragCoord.xy, 0.0)).x - 0.5) / 255.0; // dither against 8-bit banding
    outColor = vec4(encoded, 1.0);
}
