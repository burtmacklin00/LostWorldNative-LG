@echo off
rem Lost World Native LG: plays while saving the shot popups (ECHO, HIT!,
rem LONG RANGE...) for making Jurassic Park font versions of them: a 2D
rem capture in out\2d each time one shows (80 at most), and every new 3D
rem texture in out\textures. Play stage 1 for a few minutes, hitting
rem dinosaurs, then quit (pause menu > QUIT). Delete out\2d and
rem out\textures afterwards.
cd /d "%~dp0"
start "" "LostWorld.exe" --capture-popups --dump-textures
