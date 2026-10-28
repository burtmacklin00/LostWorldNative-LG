@echo off
rem Lost World Native LG: arcade 4:3 picture with the bezel (4x3 folder).
rem Takes effect the next time the game starts.
cd /d "%~dp0"
copy /y "4x3\display.ini" "display.ini" >nul
if exist "4x3\bezel.png" copy /y "4x3\bezel.png" "bezel.png" >nul
