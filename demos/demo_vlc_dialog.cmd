@echo off
set PATH=%~dp0;%PATH%

if exist "%ProgramFiles%\VideoLAN\VLC\vlc.exe" (
	set VLC=%ProgramFiles%\VideoLAN\VLC\vlc.exe
) else (
	:: otherwise check the system path for vlc.exe
	for %%X in (vlc.exe) do (set VLC=%%~$PATH:X)
)

if "%VLC%"=="" (
    echo This demo script requires VLC ^(64-bit^).
    echo.
    pause
    goto :EOF
)

:: show settings dialog, then immediately quit
"%VLC%" -I dummy dshow:// :dshow-vdev="ScreenCam" :dshow-config=true --playlist-autostart --no-video --stop-time=0.001f vlc://quit

:: play ScreenCam in VLC
"%VLC%" dshow:// :dshow-vdev="ScreenCam" --playlist-autostart
