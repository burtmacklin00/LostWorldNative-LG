@echo off
rem Lost World Native LG: native 16:9 widescreen, no bezel (16x9 folder).
rem Takes effect the next time the game starts.
cd /d "%~dp0"
copy /y "16x9\display.ini" "display.ini" >nul
rem The bezel copied in by "Display - 4x3.bat" (the original stays in 4x3)
if exist "4x3\bezel.png" if exist "bezel.png" del /q "bezel.png"
