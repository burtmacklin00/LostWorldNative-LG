@echo off
rem Lost World Native LG: plays the game normally, but saves a debug snapshot
rem (as F9) every 5 seconds while someone plays, 60 at most, in out\snap.
rem For reporting a problem without a keyboard: play until it shows up, then
rem quit (pause menu > QUIT). Each snapshot is about 20 MB: delete out\snap after.
cd /d "%~dp0"
start "" "LostWorld.exe" --snap-every 5
