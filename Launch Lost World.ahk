; Lost World Native LG - launcher for LaunchBox (AutoHotkey v1.1)
;
; - Starts LostWorld.exe from this folder
; - Esc closes the game completely (back to LaunchBox)
;   (in the game, 1 / a gun's start button opens the pause menu instead)
; - When the game ends, closes Hook of the Reaper if it is running (it is never
;   started here) with a normal close, never a forced kill
;
; Put this script next to LostWorld.exe.

#NoEnv
#SingleInstance Force
#NoTrayIcon
SetWorkingDir %A_ScriptDir%
SetBatchLines, -1

Game := A_ScriptDir . "\LostWorld.exe"
if (!FileExist(Game)) {
    MsgBox, 16, Lost World Native LG, LostWorld.exe was not found next to this script.
    ExitApp
}

Run, "%Game%", %A_ScriptDir%, UseErrorLevel, GamePID
if (ErrorLevel = "ERROR")
    ExitApp

; Bring the game window to the front once it exists
WinWait, ahk_pid %GamePID%, , 30
if !ErrorLevel
    WinActivate, ahk_pid %GamePID%

SetTimer, WatchGame, 500
return

; Esc, only while the game is the active window: close the game
#IfWinActive ahk_exe LostWorld.exe
$Esc::
    Gosub, CloseGame
return
#IfWinActive

; The game ended on its own (its Quit menu, the window's close button...)
WatchGame:
    Process, Exist, %GamePID%
    if !ErrorLevel
        Gosub, Finish
return

CloseGame:
    SetTimer, WatchGame, Off
    ; A normal close: the game saves its settings and exits by itself
    WinClose, ahk_pid %GamePID%
    Process, WaitClose, %GamePID%, 10
    if ErrorLevel                       ; still running after 10 s: stopped
        Process, Close, %GamePID%
    Gosub, Finish
return

Finish:
    SetTimer, WatchGame, Off
    Gosub, CloseHookOfTheReaper
    ExitApp
return

; Closes Hook of the Reaper only if it is already running: asks each of its
; windows (also the hidden / tray ones) to close, and waits for it. No kill.
CloseHookOfTheReaper:
    Process, Exist, HookOfTheReaper.exe
    HotrPID := ErrorLevel
    if !HotrPID
        return
    DetectHiddenWindows, On
    WinGet, HotrWindows, List, ahk_pid %HotrPID%
    Loop, %HotrWindows% {
        Win := HotrWindows%A_Index%
        WinClose, ahk_id %Win%
    }
    Process, WaitClose, %HotrPID%, 5
    DetectHiddenWindows, Off
return
