@echo off
rem Lost World Native LG: arcade 4:3 picture with the bezel.
rem Only display.ini is copied: the bezel is read straight from the 4x3 folder
rem (your 4x3\bezel.png if there is one, else 4x3\bezel-default.png).
rem Takes effect the next time the game starts.
cd /d "%~dp0"
copy /y "4x3\display.ini" "display.ini" >nul
rem Bezel copies left in the game folder by older versions of this file
if exist "4x3\bezel.png" if exist "bezel.png" del /q "bezel.png"
if exist "4x3\bezel-default.png" if exist "bezel-default.png" del /q "bezel-default.png"
