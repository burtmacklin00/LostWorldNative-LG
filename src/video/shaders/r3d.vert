#version 450
// Real3D polygons. Vertices arrive already in view space (GL convention);
// this only projects them with the viewport's reverse-Z frustum.
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_nrm;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_color;   // RGBA8 unorm
layout(location = 4) in uint in_tex;
layout(location = 5) in uint in_flags;
layout(location = 6) in vec3 in_params;  // fixed shade, fog intensity, specular coefficient
layout(location = 7) in uint in_replace; // texture pack slot (0 = original texture)
layout(location = 8) in vec3 in_prev;    // position in the previous game frame

layout(push_constant) uniform Viewport {
    mat4 proj;
    vec4 sun;        // xyz direction (view space), w intensity
    vec4 fog_color;  // rgb, a = fog ambient
    vec4 params;     // ambient, fog density, fog start, interpolation of the previously
                     // displayed image (motion vectors for DLAA)
    vec4 quality;    // MSAA samples (negative in the translucent pass, -1000 in its depth
                     // pre-pass), max anisotropic taps
                     // (0 = no mipmaps), HUD shift (NDC), interpolation between the previous
                     // and current game frame
} vp;

layout(location = 0) out vec3 view_pos;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec2 uv;
layout(location = 3) out vec4 color;
layout(location = 4) flat out uint tex;
layout(location = 5) flat out uint flags;
layout(location = 6) out vec3 params;
layout(location = 7) flat out uint replace;
layout(location = 8) out vec4 clip_now;    // this image and the previous one, for motion vectors
layout(location = 9) out vec4 clip_before;
// The translucent pass must land exactly on the depth its pre-pass wrote
invariant gl_Position;

void main() {
    // Displays faster than the game: positions in between its frames
    vec3 pos = mix(in_prev, in_pos, vp.quality.w);
    view_pos = pos;
    normal = in_nrm;
    uv = in_uv;
    color = in_color;
    tex = in_tex;
    flags = in_flags;
    params = in_params;
    replace = in_replace;
    gl_Position = vp.proj * vec4(pos, 1.0);
    vec4 before = vp.proj * vec4(mix(in_prev, in_pos, vp.params.w), 1.0);
    // HUD elements tagged by the scene builder go to the widescreen edges
    float shift = (in_flags & 0x4000u) != 0u ? -vp.quality.z : (in_flags & 0x8000u) != 0u ? vp.quality.z : 0.0;
    gl_Position.x += shift * gl_Position.w;
    before.x += shift * before.w;
    clip_now = gl_Position;
    clip_before = before;
}
