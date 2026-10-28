// Widescreen aim: the guns reach the whole 16:9 (or 21:9) picture, side areas
// included, so enemies there can be shot.
//
// The game turns a gun's ADC reading into a screen position with the gun
// calibration in its settings (RAM copy of the EEPROM at 0x121C, player 1 at
// +0x20.., player 2 at +0x26..): x = (adc - centre) * 232 / (edge - centre),
// with the right edge at +2 and the left edge at +3 (fn at 0xCBF0 and the
// other readers of the ADC table at 0xE9C). With the factory calibration the
// 0..255 ADC range only reaches a little past the 4:3 screen. While this is
// on, the horizontal calibration in RAM is replaced by a narrower one (centre
// 128, edges +-d) so 0..255 spans the whole wide picture, and the gun
// positions are converted to it: the game sees a side-area shot where it
// really is. The EEPROM is never written; the vertical calibration is kept.
#pragma once

namespace wide_aim {

// At start: on for a wide picture of that aspect (16:9 = 1.78), off for 4:3
void configure(bool on, float aspect);
bool active();

// Game thread, every frame: keeps the widened calibration in RAM (after boot),
// taking the game's own values as the factory ones whenever it reloads them
void frame(unsigned long long game_frame);

// A gun's X ADC value worked out with the factory calibration -> the value
// for the widened one (unchanged while off or not yet in place)
float remap_x(int player, float factory_adc);

}  // namespace wide_aim
