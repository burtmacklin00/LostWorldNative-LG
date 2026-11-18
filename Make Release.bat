@echo off
rem Lost World Native LG: makes a version of the game to give out, from what
rem is built here. Run it when a build is tested and ready, with its number:
rem   Make Release.bat 2.0
rem In "Update Packs" it makes
rem   LostWorld LG v2.0 (full)         every release file, for a new copy
rem   LostWorld LG v1.0 to v2.0        only what differs from the version
rem                                    before, for a copy that has it
rem (each a folder and a .zip) and records v2.0 in Update Packs\Versions.
rem Test updates (Make Update Pack.bat) go on from this version.
cd /d "%~dp0"
set "VER=%~1"
if "%VER%"=="" set /p "VER=Version number (for example 2.0): "
if "%VER%"=="" goto :end
rem The version shown in the game (main menu, LostWorld.exe's properties) comes
rem from the VERSION file: a new number is written there and the game rebuilt
set "OLDVER="
if exist VERSION set /p "OLDVER="<VERSION
if not "%OLDVER%"=="%VER%" (
    echo Setting the game's version to %VER% and building it...
    >VERSION echo(%VER%
    set "LG_NO_PAUSE=1"
    call build.bat || goto :end
)
powershell -NoProfile -ExecutionPolicy Bypass -File "tools\make_update_pack.ps1" -Release "%VER%"
:end
echo.
pause
