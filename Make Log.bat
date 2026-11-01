@echo off
rem Lost World Native LG: starts the game and saves what it reports, for
rem tracking down a start-up problem. Put this file next to LostWorld.exe and
rem double-click it. When the game has closed (or failed), send these 3 files
rem from the folder named "out":
rem   game_log.txt     what the game reported
rem   vulkan_info.txt  the graphics card and driver, as Vulkan sees them
rem   event_log.txt    Windows' error reports: the game's crashes and the
rem                    graphics driver's errors (this run and the last 7 days)
cd /d "%~dp0"
if not exist out mkdir out

echo Checking the graphics card...
where vulkaninfo >nul 2>&1 && (vulkaninfo --summary > "out\vulkan_info.txt" 2>&1) || (echo vulkaninfo was not found > "out\vulkan_info.txt")

rem The time the game starts, to tell this run's errors from older ones
for /f "usebackq delims=" %%t in (`powershell -NoProfile -Command "(Get-Date).ToString('o')"`) do set "LW_START=%%t"

echo Starting the game. Its log goes to out\game_log.txt
"LostWorld.exe" > "out\game_log.txt" 2>&1
echo Game exit code: %errorlevel% >> "out\game_log.txt"

echo Reading the Windows error reports...
rem (Windows can take a few seconds to write a crash report)
timeout /t 5 /nobreak >nul
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$start = [datetime]::Parse($env:LW_START);" ^
  "$since = (Get-Date).AddDays(-7);" ^
  "function Get-Errors($log, $test) { try { Get-WinEvent -FilterHashtable @{LogName=$log; StartTime=$since} -ErrorAction Stop | Where-Object $test } catch { @() } };" ^
  "$game = Get-Errors 'Application' { $_.Message -match 'LostWorld' };" ^
  "$gpu = Get-Errors 'System' { $_.ProviderName -match 'nvlddmkm|amdkmdag|amdwddmg|igfx|Display' -or $_.Message -match 'Display driver' };" ^
  "$out = @();" ^
  "$out += 'Game started: ' + $start;" ^
  "$out += '';" ^
  "foreach ($set in @(@('THE GAME (Application log)', $game), @('THE GRAPHICS DRIVER (System log)', $gpu))) {" ^
  "  $out += '==== ' + $set[0] + ' ====';" ^
  "  $list = @($set[1]);" ^
  "  if ($list.Count -eq 0) { $out += '(nothing in the last 7 days)' }" ^
  "  foreach ($e in ($list | Select-Object -First 20)) {" ^
  "    $tag = if ($e.TimeCreated -ge $start) { '[THIS RUN] ' } else { '' };" ^
  "    $out += '---- ' + $tag + $e.TimeCreated + '  ' + $e.ProviderName + '  (event ' + $e.Id + ', ' + $e.LevelDisplayName + ')';" ^
  "    $out += $e.Message; $out += '' } ; $out += '' };" ^
  "$out | Out-File -Encoding utf8 'out\event_log.txt'"

echo.
echo Done. Please send these 3 files from the out folder:
echo   game_log.txt, vulkan_info.txt and event_log.txt
pause
