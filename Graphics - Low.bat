@echo off
rem Lost World Native LG: LOW graphics, the lightest settings there are, so the
rem game runs on as many PCs as possible (laptops, older or built-in graphics).
rem Every costly effect is off, the 3D is drawn at half resolution without edge
rem smoothing, and only at the game's own rate. The HUD and the text stay sharp.
rem Takes effect the next time the game starts. Close the game first.
setlocal
set "SET_resolution_3d=50"
set "SET_half_rate=1"
set "SET_vsync=1"
set "SET_frame_cap=0"
set "SET_motion=original"
set "SET_aa=none"
set "SET_ray_tracing=off"
set "SET_ao=0"
set "SET_shadows=0"
set "SET_sun=0"
set "SET_light_shafts=0"
set "SET_soft_particles=0"
set "SET_sky=original"
set "SET_surface_detail=0"
set "SET_bloom=off"
set "SET_hdr=0"
set "SET_rim_light=0"
set "SET_sharpen=0"
set "SET_haze=0"
set "SET_lightning=0"
set "SET_rain=off"
set "SET_flashlight=original"
set "SET_random_weather=0"
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
