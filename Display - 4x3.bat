@echo off
rem Lost World Native LG: arcade 4:3 picture with the bezel (4x3 folder).
rem Your own 4x3\bezel.png is used if there is one, else 4x3\bezel-default.png.
rem Takes effect the next time the game starts.
cd /d "%~dp0"
copy /y "4x3\display.ini" "display.ini" >nul
if exist "4x3\bezel-default.png" copy /y "4x3\bezel-default.png" "bezel-default.png" >nul
if exist "4x3\bezel.png" (copy /y "4x3\bezel.png" "bezel.png" >nul) else if exist "bezel.png" del /q "bezel.png"
