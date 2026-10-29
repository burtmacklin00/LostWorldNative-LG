#pragma once
// Weather laid over the game's own light (not in the original), for the modes
// that pick one at random (runtime/weather.h): the sun dimmed by clouds, a
// grey sky, fog, rain and lightning. Only outdoors in daylight: where the
// backdrop is black (caves, corridors, the night) the game's light stays as it is.

struct R3DWeather {
    float sun = 1, ambient = 1;   // multipliers on the game's sun and ambient light
    float overcast = 0;           // 0..1: the backdrop (sky) turned to a cloud grey...
    float sky[3] = {0.62f, 0.65f, 0.68f};   // ... of this colour
    float fog = 0;                // fog density (per view unit, as the game's); the game's own when thicker
    float fog_color[3] = {0.6f, 0.63f, 0.66f};
    float rain = 0;               // 0..1: how much it rains (RAIN option's drops, the world wet)
    float storm = 0;              // 0..1: lightning (LIGHTNING option) above 0.5

    // The presets of runtime/weather.h, in its order (anything else: the game's own)
    static R3DWeather preset(int id) {
        R3DWeather w;
        switch (id) {
        case 0:   // SUNNY: a little more sun
            w.sun = 1.1f;
            break;
        case 1:   // CLOUDY: soft light, a grey sky, a light haze
            w.sun = 0.45f, w.ambient = 1.15f, w.overcast = 0.6f;
            w.fog = 0.0006f;
            break;
        case 2:   // FOG: pale and thick
            w.sun = 0.35f, w.ambient = 1.1f, w.overcast = 0.85f;
            w.sky[0] = w.fog_color[0] = 0.70f, w.sky[1] = w.fog_color[1] = 0.73f, w.sky[2] = w.fog_color[2] = 0.74f;
            w.fog = 0.0018f;
            break;
        case 3:   // RAIN
            w.sun = 0.3f, w.overcast = 0.75f;
            w.sky[0] = w.fog_color[0] = 0.42f, w.sky[1] = w.fog_color[1] = 0.46f, w.sky[2] = w.fog_color[2] = 0.50f;
            w.fog = 0.0012f, w.rain = 0.6f;
            break;
        case 4:   // STORM: dark, heavy rain, lightning (its bolts show over the grey sky)
            w.sun = 0.12f, w.ambient = 0.75f, w.overcast = 0.9f;
            w.sky[0] = w.fog_color[0] = 0.20f, w.sky[1] = w.fog_color[1] = 0.22f, w.sky[2] = w.fog_color[2] = 0.26f;
            w.fog = 0.0016f, w.rain = 1, w.storm = 1;
            break;
        default: break;
        }
        return w;
    }
};
