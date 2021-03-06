@echo off
set PATH=%~dp0;%PATH%

for %%X in (ffplay.exe) do (set FFPLAY_FOUND=%%~$PATH:X)

if "%FFPLAY_FOUND%"=="" (
    echo This demo script requires FFplay ^(64-bit^). Please put ffplay.exe
    echo into the same folder or the system path, and then run it again.
    echo.
    pause
    goto :EOF
)

:: show settings dialog, then immediately quit
ffplay -v 0 -f dshow -i video="ScreenCam" -show_video_device_dialog true -vn

:: play ScreenCam in FFplay
ffplay -v 0 -f dshow -i video="ScreenCam"
