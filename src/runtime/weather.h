// Random weather (not in the original) for CLASSIC > RANDOM SPAWNS, a new one
// each stage, and for survival, a new one each run: sunny, cloudy, fog, rain
// or a storm, drawn by the renderer over the game's own light
// (video/r3d_weather.h, whose presets go in this order).
#pragma once

enum Weather { WEATHER_SUNNY, WEATHER_CLOUDY, WEATHER_FOG, WEATHER_RAIN, WEATHER_STORM, WEATHER_COUNT };
constexpr int WEATHER_NONE = -1;   // the game's own

const char* weather_name(int w);   // "SUNNY" ...; nullptr for WEATHER_NONE
int weather_from_name(const char* s);   // "sunny" ... (any case), WEATHER_NONE if unknown
// A new weather at random (weighted, never the same twice in a row) for the
// modes that have one: any thread
void weather_roll();
int weather_rolled();   // the last one rolled
void weather_force(int w);   // --weather: this one instead (the rolls are not logged)
