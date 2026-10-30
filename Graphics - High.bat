@echo off
rem Lost World Native LG: HIGH graphics: mid-range cards (RTX 3060, RX 6600). Full resolution and
rem frame rate, ambient occlusion, sun shadows, light shafts and the procedural
rem sky; no ray tracing.
rem Each step up (Low, Medium, High, Ultra) turns on more. VSYNC stays on and
rem FRAME CAP off (VSYNC already matches the monitor; the game's speed never
rem changes); HALF FRAME RATE (Low, Medium) only acts above 100 Hz.
rem Takes effect the next time the game starts (edge smoothing too). Close the game first.
setlocal
set "SET_resolution_3d=100"
set "SET_half_rate=0"
set "SET_vsync=1"
set "SET_frame_cap=0"
set "SET_motion=smooth"
set "SET_aa=msaa"
set "SET_ray_tracing=off"
set "SET_ao=1"
set "SET_shadows=1"
set "SET_sun=1"
set "SET_light_shafts=1"
set "SET_soft_particles=1"
set "SET_sky=procedural"
set "SET_surface_detail=1"
set "SET_bloom=medium"
set "SET_hdr=1"
set "SET_rim_light=1"
set "SET_sharpen=1"
set "SET_haze=1"
set "SET_lightning=1"
set "SET_rain=heavy"
set "SET_flashlight=modern"
set "SET_random_weather=1"
set "KEYS=resolution_3d half_rate vsync frame_cap motion aa ray_tracing ao shadows sun light_shafts soft_particles sky surface_detail bloom hdr rim_light sharpen haze lightning rain flashlight random_weather"
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
