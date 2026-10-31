@echo off
rem Lost World Native LG: in-game captions in the port's standard sharp fonts
rem (Saira, Barlow, Share Tech Mono), as the creator made them.
rem Copies textures\text_original into textures\text. Takes effect the next
rem time the game starts.
cd /d "%~dp0"
if not exist "textures\text_original\atlas.png" (
    echo textures\text_original is missing.
    pause
    exit /b 1
)
if not exist "textures\text" mkdir "textures\text"
copy /y "textures\text_original\atlas.png" "textures\text\atlas.png" >nul
copy /y "textures\text_original\glyphs.txt" "textures\text\glyphs.txt" >nul
echo In-game text: original sharp fonts.
