@echo off
rem Double-click this. Installs anything missing, then builds LostWorld.exe.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-and-build.ps1"
echo.
pause
