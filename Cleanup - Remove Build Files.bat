@echo off
rem Lost World Native LG: removes the source code and build files from a
rem copy of the finished game, leaving only what is needed to play.
rem Run it in the COPY (for example G:\LostWorld Release Build), never in
rem the folder you build and update in: that one needs these files.
setlocal
cd /d "%~dp0"

if not exist "LostWorld.exe" (
    echo LostWorld.exe is not in this folder: build the game first.
    goto :end
)
if not exist "data\crom.bin" (
    echo The data folder is missing ^(data\crom.bin^): the game would not start
    echo without it or lostwsga.zip. Nothing was deleted.
    goto :end
)

echo This deletes the source code and build files from:
echo   %CD%
echo.
echo Removed: .git, .github, build, docs, src, tests, tools, the build
echo libraries in third_party, build.bat, setup-and-build, update.bat,
echo Make Update Pack.bat, Update Packs, the Debug bats, out (debug
echo snapshots), an update's _UPDATE - READ ME.txt,
echo CMakeLists.txt and README.md.
echo Kept: LostWorld.exe, data, save, your settings, guns, bezels, menu art,
echo fonts, Hook of the Reaper, the launcher and the setting .bat files.
echo.
echo Do NOT run this in the folder you build and update the game in.
echo.
set "ANSWER="
set /p "ANSWER=Type YES to delete them: "
if /i not "%ANSWER%"=="YES" (
    echo Nothing was deleted.
    goto :end
)

for %%d in (".git" ".github" "build" "docs" "src" "tests" "tools" "third_party\glslang" "third_party\musashi" "third_party\ngx" "third_party\volk" "third_party\vulkan-headers" "Update Packs" "out") do (
    if exist "%%~d\" (
        echo Deleting %%~d
        attrib -r -h -s "%%~d\*" /s /d >nul 2>&1
        rd /s /q "%%~d"
    )
)
for %%f in (".gitattributes" ".gitignore" "build.bat" "CMakeLists.txt" "README.md" "setup-and-build.bat" "setup-and-build.ps1" "update.bat" "update.bat.new" "Make Update Pack.bat" "Debug - Auto Snapshots.bat" "Debug - Capture Popups.bat" "Debug - Dump Textures.bat" "_UPDATE - READ ME.txt" "res\lostworld.rc" "res\lostworld.ico" "res\logo.png") do (
    if exist "%%~f" (
        echo Deleting %%~f
        del /f /q "%%~f"
    )
)

if exist "Getting Started - Release Copy.txt" (
    echo Using the play-only readme
    move /y "Getting Started - Release Copy.txt" "1 - Getting Started Readme.txt" >nul
)

echo.
echo Done. The game is ready to play: Launch Lost World.ahk or LostWorld.exe.

:end
echo.
pause
