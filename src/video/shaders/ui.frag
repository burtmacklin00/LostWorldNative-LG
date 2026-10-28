#version 450
// Menu layer (premultiplied alpha) drawn over the finished picture, with the
// menu pointer drawn as player 1's crosshair (the same ring as in the game)
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform sampler2D ui;
layout(push_constant) uniform Cursor {
    vec4 cursor;   // xy: pointer in window pixels (negative: none); z: window height
} pc;

// compose.frag's crosshair(): ring and cross with a dark outline
vec4 crosshair(vec2 frag, vec2 at, vec3 tint) {
    vec2 d = frag - at;
    float size = max(10.0, pc.cursor.z / 60.0);
    float r = length(d);
    float ring = abs(r - size);
    float cross = min(abs(d.x), abs(d.y));
    bool in_cross = max(abs(d.x), abs(d.y)) < size * 1.6 && r > size * 0.35;
    float core = min(ring, in_cross ? cross : 1e9);
    if (core < 1.2) return vec4(tint, 1.0);
    if (core < 3.0) return vec4(0.0, 0.0, 0.0, 0.85);   // outline
    if (r < 1.8) return vec4(tint, 1.0);                // centre dot
    return vec4(0.0);
}

void main() {
    color = texture(ui, uv);
    if (pc.cursor.x >= 0.0) {
        vec4 x = crosshair(gl_FragCoord.xy, pc.cursor.xy, vec3(1.0, 0.95, 0.2));
        color = vec4(x.rgb * x.a, x.a) + color * (1.0 - x.a);
    }
}
