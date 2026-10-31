@echo off
rem Lost World Native LG: in-game captions in the Jurassic Park font.
rem FREE PLAY, INSERT COIN, CREDIT, the shot popups (ECHO, HIT...) and the
rem other letters the game draws get the Jurassic Park font; digits and
rem punctuation keep their font (the Jurassic Park font has none).
rem Copies textures\text_jurassic into textures\text. Needs Settings > HD TEXT
rem on. Takes effect the next time the game starts.
cd /d "%~dp0"
if not exist "textures\text_jurassic\atlas.png" (
    echo textures\text_jurassic is missing.
    pause
    exit /b 1
)
if not exist "textures\text" mkdir "textures\text"
copy /y "textures\text_jurassic\atlas.png" "textures\text\atlas.png" >nul
copy /y "textures\text_jurassic\glyphs.txt" "textures\text\glyphs.txt" >nul
echo In-game text: Jurassic Park font.
