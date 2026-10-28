@echo off
rem Double-click to get the latest Lost World Native LG version and rebuild.
rem Installs Python 3 and the VS 2022 Build Tools first if they're missing
rem (uses setup-and-build.ps1, which must sit next to this file).
rem No Git needed: downloads the latest ZIP from GitHub with the curl and tar
rem built into Windows 10/11 and copies the new files over this folder.
rem Your ROM zip, data\, build\, saves, textures and LostWorld.exe are kept.
rem Run "update.bat force" to download and rebuild even if nothing changed.
setlocal
cd /d "%~dp0"

set "REPO=burtmacklin00/LostWorldNative-LG"
set "BRANCH=master"
set "TMPDIR=%TEMP%\lw_update"

if not exist build.bat (
    echo Put update.bat in the LostWorldNative folder, next to build.bat.
    goto fail
)

rem --- Is there anything new? Asks GitHub for the latest commit id ----------
set "LATEST="
for /f "usebackq delims=" %%s in (`curl -fsSL -H "Accept: application/vnd.github.sha" https://api.github.com/repos/%REPO%/commits/%BRANCH%`) do set "LATEST=%%s"
set "CURRENT="
if exist .lw_version set /p CURRENT=<.lw_version

if /i not "%~1"=="force" if defined LATEST if "%LATEST%"=="%CURRENT%" (
    echo Already up to date. Nothing to do.
    echo Run "update.bat force" to download and rebuild anyway.
    pause
    exit /b 0
)
if not defined LATEST echo Could not check the latest version, updating anyway.

rem --- Install anything the rebuild needs (Python 3, VS 2022 Build Tools) -----
rem Done before touching any files, so a failed install leaves the game as it is
if not exist setup-and-build.ps1 (
    echo setup-and-build.ps1 is missing. It needs to be in this folder next to update.bat.
    goto fail
)
echo === Checking build tools ===
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-and-build.ps1" -PrereqsOnly
if errorlevel 1 goto fail
echo.

rem --- Download and extract --------------------------------------------------
echo === Downloading the latest version from GitHub ===
if exist "%TMPDIR%" rmdir /s /q "%TMPDIR%"
mkdir "%TMPDIR%"
curl -fL --progress-bar -o "%TMPDIR%\src.zip" https://github.com/%REPO%/archive/refs/heads/%BRANCH%.zip || (
    echo Download failed. Check your internet connection.
    goto fail
)
tar -xf "%TMPDIR%\src.zip" -C "%TMPDIR%" || (
    echo Could not extract the download.
    goto fail
)
set "NEW=%TMPDIR%\LostWorldNative-LG-%BRANCH%"
if not exist "%NEW%\build.bat" (
    echo The download did not look right ^(build.bat missing^).
    goto fail
)

rem --- Copy the new files over -----------------------------------------------
echo === Updating files ===
rem Code folders are mirrored exactly, so files the creator deleted go away too
for %%d in (src tools tests docs .github third_party\glslang third_party\musashi third_party\volk third_party\vulkan-headers) do (
    if exist "%NEW%\%%d" (
        robocopy "%NEW%\%%d" "%%d" /MIR /NFL /NDL /NJH /NJS /NP >nul
        if errorlevel 8 ( echo Copy failed for %%d & goto fail )
    )
)
rem These folders also hold files you add yourself (your own 4x3\bezel.png...),
rem so only add/replace, never delete
for %%d in (res third_party\fonts 4x3 16x9) do (
    if exist "%NEW%\%%d" (
        robocopy "%NEW%\%%d" "%%d" /E /NFL /NDL /NJH /NJS /NP >nul
        if errorlevel 8 ( echo Copy failed for %%d & goto fail )
    )
)
rem Top-level files (build.bat, CMakeLists.txt, README.md...), not subfolders
rem (not update.bat itself: Windows reads a running .bat as it goes, so it is
rem replaced only at the very end, from update.bat.new)
robocopy "%NEW%" "." /XF update.bat /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 ( echo Copy failed for the top-level files & goto fail )
copy /y "%NEW%\update.bat" "update.bat.new" >nul
rmdir /s /q "%TMPDIR%"

rem --- Rebuild ------------------------------------------------------------------
echo.
echo === Rebuilding ===
rem build.bat pauses at the end on its own when started by a double-click
call build.bat
if errorlevel 1 goto swap_self_failed
if defined LATEST (> .lw_version echo %LATEST%)
rem One parsed block: replaces this file and ends without reading it again
if exist update.bat.new (move /y update.bat.new update.bat >nul & exit /b 0)
exit /b 0

:swap_self_failed
if exist update.bat.new (move /y update.bat.new update.bat >nul & exit /b 1)
exit /b 1

:fail
if exist "%TMPDIR%" rmdir /s /q "%TMPDIR%"
echo.
echo Update failed. Your current copy of the game is unchanged unless the
echo messages above say otherwise.
pause
exit /b 1
