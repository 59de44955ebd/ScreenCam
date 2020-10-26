@echo off
set PATH=%~dp0;%PATH%

:: check if GraphStudioNext64.exe can be found
for %%X in (GraphStudioNext64.exe) do (set GSN_FOUND=%%~$PATH:X)
if "%GSN_FOUND%"=="" (
    echo This demo script requires GraphStudioNext ^(64-bit^). Please put GraphStudioNext64.exe
    echo into the same folder or the system path, and then run it again.
    echo.
    echo You can download a binary from:
    echo https://github.com/59de44955ebd/graph-studio-next-enhanced/blob/master/bin/x64/Release/GraphStudioNext64.exe
    echo.
    pause
    goto :EOF
)

:: create a temporary graph XML file
set GML="%TMP%\~cam.xml"
echo ^<?xml version="1.0" encoding="utf-8"?^> > %GML%
echo ^<graph name="ScreenCam Graph" clock="none"^> >> %GML%
echo ^<filter name="Video Renderer" index="0" clsid="{B87BEB7B-8D29-423F-AE4D-6582C10175AC}"^>^</filter^> >> %GML%
echo ^<filter name="Color Space Converter" index="1" clsid="{1643E180-90F5-11CE-97D5-00AA0055595A}"/^> >> %GML%
echo ^<filter name="ScreenCam" index="2" clsid="{69168CC9-E263-46C7-9F6C-7BB51FFCA6CB}"/^> >> %GML%
echo ^<connect out="ScreenCam/Capture" in="Color Space Converter/Input"^>^</connect^> >> %GML%
echo ^<connect out="Color Space Converter/XForm Out" in="Video Renderer/VMR Input0"^>^</connect^> >> %GML%
echo ^</graph^> >> %GML%

:: load and run it in GraphStudioNext
GraphStudioNext64.exe %GML% -run

:: delete the tmp file
del %GML%
