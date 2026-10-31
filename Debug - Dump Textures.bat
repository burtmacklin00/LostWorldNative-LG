@echo off
rem Lost World Native LG: plays with --dump-textures. Every new 3D texture the
rem game shows is written once to out\textures (for making replacements, such
rem as the shot popups ECHO / HIT in the Jurassic Park font). Play a minute of
rem stage 1 shooting dinosaurs, then quit (pause menu > QUIT).
cd /d "%~dp0"
start "" "LostWorld.exe" --dump-textures
