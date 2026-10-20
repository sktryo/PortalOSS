@echo off
setlocal
cd /d "%~dp0"

devtools\bin\vpc.exe /portal /define:SOURCESDK +everything /mksln portal.sln
if errorlevel 1 (
    echo.
    echo Project generation failed.
    exit /b 1
)

echo.
echo Generated src\portal.sln
endlocal
