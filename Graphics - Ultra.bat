@echo off
rem Lost World Native LG: ULTRA graphics (everything on, ray-traced shadows and ambient occlusion).
rem Takes effect the next time the game starts. Close the game first.
setlocal
set "SET_resolution_3d=100"
set "SET_half_rate=0"
set "SET_ray_tracing=ao"
set "SET_ao=1"
set "SET_shadows=1"
set "SET_light_shafts=1"
set "SET_soft_particles=1"
set "SET_sky=procedural"
set "SET_surface_detail=1"
set "SET_bloom=medium"
set "KEYS=resolution_3d half_rate ray_tracing ao shadows light_shafts soft_particles sky surface_detail bloom"
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
