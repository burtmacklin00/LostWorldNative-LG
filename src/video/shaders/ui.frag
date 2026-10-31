#version 450
// Menu layer (premultiplied alpha) drawn over the finished picture, with the
// menu pointer drawn as player 1's crosshair (the same ring as in the game).
//
// Settings > MENU EFFECTS: the menu backdrop comes alive. The picture marks
// its own pixels in alpha (ui/frontend.cpp): 254 the jungle backdrop, 253 the
// logo, 255 everything drawn over them (text, panels, buttons), which is left
// alone. On the backdrop: leaves stirring in the wind, a slow camera drift,
// drifting mist and light rays through the trees; on the logo, a glint now
// and then. A handful of operations per pixel, only while a menu with the
// backdrop is open.
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

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) {   // three octaves: soft, cheap
    return 0.55 * noise(p) + 0.3 * noise(p * 2.03 + 1.7) + 0.15 * noise(p * 4.1 + 3.1);
}

vec3 backdrop_fx(vec3 c, vec2 p, float aspect, float t, bool logo) {
    // Light rays: soft warm bands slanting down from the top left, slowly swaying
    vec2 q = vec2(p.x * aspect, p.y);
    float along = q.x * 0.82 + q.y * 0.57;
    float rays = 0.0;
    rays += smoothstep(0.55, 1.0, sin(along * 9.0 + sin(t * 0.13) * 1.4)) * 0.6;
    rays += smoothstep(0.6, 1.0, sin(along * 15.0 - t * 0.07 + 2.0)) * 0.4;
    rays *= (1.0 - p.y) * smoothstep(0.0, 0.25, 1.0 - abs(p.x - 0.35) * 1.6);
    rays *= 0.55 + 0.45 * fbm(vec2(along * 3.0, t * 0.05));
    // Mist: low, drifting slowly sideways
    float mist = fbm(vec2(q.x * 2.2 + t * 0.03, q.y * 3.0 - t * 0.01));
    mist = smoothstep(0.35, 0.85, mist) * smoothstep(0.35, 1.0, p.y);
    if (!logo) {
        c += vec3(1.0, 0.85, 0.55) * rays * 0.14;
        c = mix(c, vec3(0.55, 0.62, 0.55), mist * 0.18);
    }
    return c;
}

void main() {
    color = texture(ui, uv);
    float a = color.a * 255.0;
    bool back = abs(a - 254.0) < 0.5, logo = abs(a - 253.0) < 0.5;
    if (back || logo) color.a = 1.0;   // (the marks are opaque backdrop)
    if (pc.fx.y > 0.5) {
        if (back || logo) {
            float t = pc.fx.x;
            vec2 size = vec2(textureSize(ui, 0));
            float aspect = size.x / size.y;
            vec3 c = color.rgb;
            if (back) {
                // The leaves stir in the wind: the jungle warped by slow
                // noise, in gusts, more at the edges than in the middle;
                // and the camera drifts: a slow zoom and pan
                float gust = 0.55 + 0.45 * sin(t * 0.37) * sin(t * 0.11 + 1.0);
                vec2 w = vec2(noise(uv * vec2(7.0, 5.0) + vec2(t * 0.45, t * 0.2)),
                              noise(uv * vec2(6.0, 7.0) + vec2(4.1 - t * 0.3, 1.3 + t * 0.35))) - 0.5;
                float edge = 0.45 + 0.55 * clamp(abs(uv.x - 0.45) * 2.4 + max(uv.y - 0.55, 0.0) * 1.6, 0.0, 1.0);
                vec2 sway = w * vec2(0.010, 0.007) * gust * edge;
                float z = 1.0 - 0.03 * (0.5 + 0.5 * sin(t * 0.06));
                vec2 pan = vec2(sin(t * 0.041), cos(t * 0.029)) * 0.008;
                vec4 d = texture(ui, (uv - 0.5) * z + 0.5 + pan + sway);
                if (abs(d.a * 255.0 - 254.0) < 0.5) c = d.rgb;   // (never pulls in text or the logo)
            } else {      // the logo: a glint sweeping across it every 7 seconds
                float s = fract(t / 7.0) * 3.0 - 1.0;   // -1..2: off, across, off
                float band = (uv.x * aspect * 0.6 + uv.y) - s;
                c += vec3(1.0, 0.95, 0.8) * smoothstep(0.06, 0.0, abs(band)) * 0.35;
                c *= 1.0 + 0.04 * sin(t * 1.3);   // and a gentle glow
            }
            color = vec4(clamp(backdrop_fx(c, uv, aspect, t, logo), 0.0, 1.0), 1.0);
        }
    }
    if (pc.cursor.x >= 0.0) {
        vec4 x = crosshair(gl_FragCoord.xy, pc.cursor.xy, vec3(1.0, 0.95, 0.2));
        color = vec4(x.rgb * x.a, x.a) + color * (1.0 - x.a);
    }
}
