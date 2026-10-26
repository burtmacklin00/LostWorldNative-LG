#version 450
// Ambient occlusion, second step: the raw occlusion is smoothed over 4x4
// pixels, without mixing across depth edges, and multiplied into the image
// of the game world (the colour target blends with dst * src).
#extension GL_GOOGLE_include_directive : require
#include "ao_common.glsl"

layout(location = 0) out vec4 out_factor;

void main() {
    vec2 frag = gl_FragCoord.xy;
    float d = texelFetch(depth_tex, ivec2(frag), 0).r;
    if (d <= 0.0) {
        out_factor = vec4(1.0);
        return;
    }
    float z = AO_NEAR / d;
    ivec2 size = textureSize(ao_tex, 0);
    float sum = 0.0, weight = 0.0;
    for (int y = -2; y < 2; y++)
        for (int x = -2; x < 2; x++) {
            ivec2 q = clamp(ivec2(frag) + ivec2(x, y), ivec2(0), size - 1);
            float dq = texelFetch(depth_tex, q, 0).r;
            if (dq <= 0.0) continue;
            float w = exp(-abs(AO_NEAR / dq - z) / (0.03 * z));
            sum += texelFetch(ao_tex, q, 0).r * w;
            weight += w;
        }
    float open = weight > 0.0 ? sum / weight : 1.0;
    out_factor = vec4(vec3(mix(1.0, open, ao.params.y)), 1.0);
}
