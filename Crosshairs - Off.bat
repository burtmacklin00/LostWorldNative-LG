@echo off
rem Lost World Native LG: hide the players' crosshairs.
rem Takes effect the next time the game starts. Close the game first.
setlocal
set "SET_crosshair=0"
set "KEYS=crosshair"
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
