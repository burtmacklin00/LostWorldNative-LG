@echo off
rem Lost World Native LG: native 16:9 widescreen, no bezel.
rem Takes effect the next time the game starts.
cd /d "%~dp0"
copy /y "16x9\display.ini" "display.ini" >nul
rem Bezel copies left in the game folder by older versions of these files
if exist "4x3\bezel.png" if exist "bezel.png" del /q "bezel.png"
if exist "4x3\bezel-default.png" if exist "bezel-default.png" del /q "bezel-default.png"
