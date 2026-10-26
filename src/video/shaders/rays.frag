#version 450
// Light shafts (not in the original): the visible sky around the sun
// (compose.frag, mask mode) smeared towards the sun, so it streams out
// between trees and silhouettes. Quarter size; the composition adds it.
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform sampler2D mask;

layout(push_constant) uniform Rays {
    vec4 p;   // xy: the sun in the 3D image (fractions, may be off screen); z: strength
} pc;

void main() {
    const int STEPS = 48;
    vec2 step = (uv - pc.p.xy) * (0.9 / float(STEPS));
    vec2 q = uv;
    float weight = 1.0, sum = 0.0;
    for (int i = 0; i < STEPS; i++) {
        q -= step;
        if (any(lessThan(q, vec2(0.0))) || any(greaterThan(q, vec2(1.0)))) break;
        sum += texture(mask, q).r * weight;
        weight *= 0.965;
    }
    color = vec4(vec3(sum / float(STEPS) * pc.p.z), 0.0);
}
