@echo off
rem Lost World Native LG: plays the game normally and writes what it reports
rem (the same lines a command prompt would show) to out\game_log.txt, for
rem tracking down a problem. Quit the game as usual, then send that file.
cd /d "%~dp0"
if not exist out mkdir out
echo The game is running. Its log goes to out\game_log.txt (this window closes with the game).
"LostWorld.exe" > "out\game_log.txt" 2>&1
