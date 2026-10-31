@echo off
rem Lost World Native LG: makes a drag-and-drop update for your release copy
rem of the game, with only the files that changed since the last update, in
rem "Update Packs" (a folder and a .zip). See tools\make_update_pack.ps1.
rem   Make Update Pack.bat          the files changed since the last update
rem   Make Update Pack.bat -Full    every release file
rem   Make Update Pack.bat -Baseline  record the current files as released
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "tools\make_update_pack.ps1" %*
echo.
pause
