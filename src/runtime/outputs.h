// Outputs for Hook of the Reaper (and anything else that reads MAME's network
// outputs): a TCP server on port 8000. A client that connects gets
// "mame_start = jplostworldsa", then every output's current value and each
// change as "name = value", lines ending in "\r"; "mame_stop = 1" at exit.
// Hook of the Reaper loads DefaultLG/jplostworldsa.txt for this game.
//
// Signals (players 1 and 2, P2_ the same):
//   P1_CtmRecoil  1 for each bullet actually fired (the game's shot counter),
//                 back to 0 after it. Held 2 frames for the standard gun and
//                 longer for the special guns (a heavier single kick with
//                 Recoil_Value). Never more than one kick per bullet.
//   P1_Ammo       bullets left (unchanged while a pickup gives unlimited ammo)
//   P1_Life       lives (0 when the player is out)
//   P1_Damaged    1 for a moment when a hit takes a life, not for the hit
//                 that takes the last one (that one is the death, P1_Life = 0)
//   P1_Weapon     0 standard gun, 1 / 2 / 3 the special guns (pickups)
//   P1_Special    1 while a special gun is in hand (for lights or rumble)
//   P1_Unlimited  1 while the unlimited-ammo pickup is active
//   P1_LmpStart   blinks while the player can start or continue
#pragma once

namespace outputs {

void start();                          // the server (no-op if it can't listen)
void stop();                           // mame_stop to the clients, then closes
void frame(unsigned long long game_frame);   // game thread, every frame

}  // namespace outputs
