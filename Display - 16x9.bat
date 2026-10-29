@echo off
rem Lost World Native LG: native 16:9 widescreen, no bezel.
rem Sets Settings > WIDESCREEN to 16:9 and copies 16x9\display.ini (no bezel).
rem Takes effect the next time the game starts.
setlocal
cd /d "%~dp0"
copy /y "16x9\display.ini" "display.ini" >nul
rem Bezel copies left in the game folder by older versions of these files
if exist "4x3\bezel.png" if exist "bezel.png" del /q "bezel.png"
if exist "4x3\bezel-default.png" if exist "bezel-default.png" del /q "bezel-default.png"
rem Settings > WIDESCREEN (save\settings.ini), the same as in the game's menu
set "SET_aspect=16:9"
set "KEYS=aspect"
call :apply
exit /b
rem ---------------------------------------------------------------------------
rem Rewrites save\settings.ini with the SET_<key> values above: the lines for
rem those keys are replaced (added if missing), every other line is kept.
:apply
cd /d "%~dp0"
if not exist save mkdir save
set "INI=save\settings.ini"
set "OUT=save\settings.ini.tmp"
setlocal EnableDelayedExpansion
type nul > "%OUT%"
if exist "%INI%" for /f "usebackq tokens=1,* delims==" %%a in ("%INI%") do (
    for /f "tokens=1" %%k in ("%%a") do (
        if defined SET_%%k (
            >>"%OUT%" echo %%k = !SET_%%k!
            set "DONE_%%k=1"
        ) else if "%%b"=="" (
            >>"%OUT%" echo %%a
        ) else (
            >>"%OUT%" echo %%a=%%b
        )
    )
)
for %%k in (%KEYS%) do if not defined DONE_%%k >>"%OUT%" echo %%k = !SET_%%k!
endlocal
move /y "%OUT%" "%INI%" >nul
exit /b
