#version 450
// Menu layer (premultiplied alpha) drawn over the finished picture, with the
// menu pointer drawn as player 1's crosshair (the same ring as in the game).
//
// Settings > MENU EFFECTS: a glint sweeping across the menu's logo now and
// then, and a gentle glow. The picture marks its own pixels in alpha
// (ui/frontend.cpp): 254 the backdrop, 253 the logo, 255 everything drawn over
// them (text, panels, buttons). Only the logo is touched.
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform sampler2D ui;
layout(push_constant) uniform Params {
    vec4 cursor;   // xy: pointer in window pixels (negative: none); z: window height
    vec4 fx;       // x: seconds; y: 1 = effects on
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
    float a = color.a * 255.0;
    bool back = abs(a - 254.0) < 0.5, logo = abs(a - 253.0) < 0.5;
    if (back || logo) color.a = 1.0;   // (the marks are opaque backdrop)
    if (logo && pc.fx.y > 0.5) {   // a glint sweeping across the logo every 7 seconds, and a gentle glow
        float t = pc.fx.x;
        float aspect = float(textureSize(ui, 0).x) / float(textureSize(ui, 0).y);
        float s = fract(t / 7.0) * 3.0 - 1.0;   // -1..2: off, across, off
        float band = (uv.x * aspect * 0.6 + uv.y) - s;
        vec3 c = color.rgb + vec3(1.0, 0.95, 0.8) * smoothstep(0.06, 0.0, abs(band)) * 0.35;
        c *= 1.0 + 0.04 * sin(t * 1.3);
        color = vec4(clamp(c, 0.0, 1.0), 1.0);
    }
    if (pc.cursor.x >= 0.0) {
        vec4 x = crosshair(gl_FragCoord.xy, pc.cursor.xy, vec3(1.0, 0.95, 0.2));
        color = vec4(x.rgb * x.a, x.a) + color * (1.0 - x.a);
    }
}
