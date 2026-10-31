=====================================================================
 LOST WORLD NATIVE LG  -  Getting Started
 The Lost World: Jurassic Park (Sega, 1997) arcade game, running
 natively on PC, set up for light gun cabinets.
=====================================================================

You need your own copy of the arcade ROM: lostwsga.zip (Japan, Rev. A,
Sega Model 3 Step 1.5). Nothing from the ROM is included. Needs
Windows 10 or 11 and a graphics card with up-to-date drivers (Vulkan).


---------------------------------------------------------------------
 1. FIRST-TIME SETUP
---------------------------------------------------------------------

 1. Put lostwsga.zip in this folder (next to build.bat).
 2. Double-click  setup-and-build.bat
      - Installs what the build needs if it is missing (Python 3 and
        the Visual Studio 2022 Build Tools). Click Yes if Windows asks.
      - Builds the game. The first time takes a while.
      - It ends with "Done: LostWorld.exe".
 3. Set up your light guns (next section).
 4. Play: double-click  Launch Lost World.ahk  (or LostWorld.exe).

 Every SURVIVAL arena is open from the start (Options > GAME > ALL
 ARENAS OPEN; with it off, each arena opens once you reach it in
 CLASSIC ARCADE).


---------------------------------------------------------------------
 2. LIGHT GUN SETUP
---------------------------------------------------------------------

 Double-click  1 - Light Gun Setup.exe  and pull the trigger on the
 PLAYER 1 gun, then on the PLAYER 2 gun when it asks. It saves
 LightgunHID.ini in this folder with each gun's ID (VID&PID).

 With LightgunHID.ini:
   - Each gun always belongs to its player, whichever fires first.
   - Other mice only move the pointer in Windows, not in the game.
 If none of its guns is plugged in when the game starts, the game
 uses normal mouse mode instead (the mouse is player 1; a second
 mouse joins as player 2 by clicking during a game).

 Guns must be in mouse mode (absolute position), as for MAME.
 To check them: Options > CONTROLS > GUN TEST shows every gun and
 where it points. LightgunHID.ini decides which gun is which player.


---------------------------------------------------------------------
 3. CONTROLS
---------------------------------------------------------------------

 LIGHT GUNS
   Trigger ................. Shoot
   Trigger (not playing) ... Start / continue (free play)
   Shoot off the screen .... Reload
   Right button ............ Reload
   Start button (key 1) .... Start
   During play, the trigger is also your START: when the game says
   to keep pressing START, just keep shooting.
   Special guns from pickups (sniper, shotgun...) fire with the
   normal trigger; they have no extra button.

 IN THE MENUS
   Player 1's gun moves the crosshair; the trigger selects and the
   right button goes back. Player 2's gun is ignored in the menus.

 KEYBOARD
   Tab .............. Pause menu during play (Tab again resumes)
   1 / 2 ............ Player 1 / 2 start
   5 / 6 ............ Coin for player 1 / 2 (only without free play;
                      with free play they do nothing)
   Esc .............. With the launcher: quits the game completely.
                      Without it: pause menu.
   F11, Alt+Enter ... Fullscreen on / off
   C ................ Crosshairs on / off (until the game restarts)
   F2 / F3 .......... Arcade test / service buttons
   F9 ............... Debug snapshot (out\snap), for reporting problems

 Only the light guns play (or the mouse without guns).


---------------------------------------------------------------------
 4. DISPLAY: 4:3 WITH BEZEL OR 16:9
---------------------------------------------------------------------

 Close the game, then double-click one of these. It applies the next
 time the game starts.

   Display - 4x3.bat .... The arcade's 4:3 picture with the bezel.
                          Shooting the bezel reloads, as on the
                          cabinet; the crosshair stops at the edge.
   Display - 16x9.bat ... Native widescreen. The guns reach the whole
                          width, so enemies in the side areas can be
                          shot; only pointing away from the screen
                          reloads. Each player's RELOAD / CONTINUE
                          prompts sit in their half of the screen.

 Your own bezel: save it as  4x3\bezel.png  (1920x1080 PNG with a
 transparent 4:3 opening in the middle). Without it the game uses
 4x3\bezel-default.png. Updates never replace your bezel.png.

 The .bat files also set WIDESCREEN (Options > DISPLAY), which can be
 changed in the game too: 4:3, 16:9 or 21:9 (ultrawide monitors). The
 bezel only shows with the 4:3 picture.


---------------------------------------------------------------------
 5. QUICK SETTINGS (close the game first)
---------------------------------------------------------------------

   Graphics - Low.bat ...... Lightest: laptops, older or built-in graphics
   Graphics - Medium.bat ... Entry-level cards (GTX 1060, RX 580)
   Graphics - High.bat ..... Mid-range cards (RTX 3060, RX 6600)
   Graphics - Ultra.bat .... Everything on, with ray tracing (the default)
   Each step up turns on more; slow on a PC? Go one step down.
   Crosshairs - On.bat / Crosshairs - Off.bat
   Text - Jurassic Park.bat / Text - Original.bat
      The game's captions (FREE PLAY, INSERT COIN, PUSH 1P / 2P START,
      CONTINUE, the boss and stage names...) in the Jurassic Park
      font, or the arcade's original lettering. Numbers keep their
      font, and the shot popups (ECHO, HIT!...) always keep the
      arcade's look. The menus use the Jurassic Park font either way.

 With FREE PLAY on, the game runs in the arcade's own FREE PLAY mode:
 the screen says FREE PLAY instead of CREDIT 0.

 Everything else is in the game: Options (main menu) or the pause
 menu > Settings.
   GAME tab ...... FREE PLAY, TRIGGER STARTS, ALL ARENAS OPEN
                   AMMO (cheat, NORMAL by default): AUTO RELOAD refills the
                   gun when it runs dry, UNLIMITED never runs out. Only the
                   standard gun; no records or achievements while it is on
   DISPLAY tab ... FRAME CAP (Off, 30, 60, 120 FPS), VSYNC, WIDESCREEN,
                   MENU EFFECTS (a glint across the main menu's logo;
                   Graphics - Low turns it off)
 PLAY > CLASSIC ARCADE > RANDOM SPAWNS shuffles where and when the
 dinosaurs come.


---------------------------------------------------------------------
 6. LAUNCHBOX / FRONT ENDS
---------------------------------------------------------------------

 Point LaunchBox at  Launch Lost World.ahk  (AutoHotkey 1.1).
   - Starts the game with no console window.
   - Esc quits the game completely, back to LaunchBox.
   - When the game ends it closes Hook of the Reaper if it is running
     (a normal close; it never starts it).


---------------------------------------------------------------------
 7. HOOK OF THE REAPER (RECOIL, LIGHTS)
---------------------------------------------------------------------

 1. Copy  Hook Of The Reaper\defaultLG\jplostworldsa.txt  into Hook of
    the Reaper's own defaultLG folder.
 2. Start Hook of the Reaper (TCP on port 8000), then the game.

 What the game sends:
   - Recoil: one kick per bullet actually fired (never on empty clicks,
     reloads or menu shots). The special guns from pickups give a
     heavier single kick on Recoil_Value guns (GUN4IR, OpenFire,
     Fusion) and a shake when picked up.
   - Ammo, lives, damage (not on the hit that takes the last life) and
     death, the gun in hand, and blinking start lamps.


---------------------------------------------------------------------
 8. MENU ARTWORK AND MUSIC (optional, your own files)
---------------------------------------------------------------------

 In  third_party\menu\ :
   logo.png ........... Title logo (transparent PNG); this folder has
                        the Native LG Edition logo
   background.jpg ..... Main menu background (1920x1080)
   music.mp3 .......... Menu music (Options > Audio > Menu music)
   sega.png ........... SEGA logo for the credit at the bottom
 In  third_party\fonts\ :  Jurassic_Park.ttf  (menu lettering)


---------------------------------------------------------------------
 9. UPDATING
---------------------------------------------------------------------

 Double-click  update.bat . It downloads the latest version, installs
 anything missing and rebuilds. It keeps your ROM, saves, settings,
 LightgunHID.ini, your bezel and your menu artwork.

 A play-only copy (for a cabinet): copy this whole folder, then run
 Cleanup - Remove Build Files.bat in the COPY. To update that copy
 later, run  Make Update Pack.bat  here after building: it makes an
 "Update Packs\LostWorld LG Update <date>" folder (and .zip) with only
 the changed files, to drag into the copy (see its READ ME).


---------------------------------------------------------------------
 10. TROUBLESHOOTING
---------------------------------------------------------------------

 - Guns swapped or not responding: run  1 - Light Gun Setup.exe
   again, and check the guns are plugged in before starting the game.
 - A setting .bat did nothing: the game was open; close it and run
   the .bat again.
 - Aim is off: check your guns' own calibration first (the game uses
   their position as Windows reports it).
 - No recoil: Hook of the Reaper must be running before the game,
   with jplostworldsa.txt in its defaultLG folder.
 - The build failed: run build.bat from a command prompt to see the
   error, and keep the last lines.

 More detail: README.md and docs\MANUAL.md.
