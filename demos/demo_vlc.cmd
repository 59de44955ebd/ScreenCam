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

:: play ScreenCam in VLC
"%VLC%" dshow:// :dshow-vdev="ScreenCam" --playlist-autostart
