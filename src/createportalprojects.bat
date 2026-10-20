@echo off
setlocal
cd /d "%~dp0"

rem Generate a solution containing only the Portal mod DLLs/exe and the static
rem lib projects they link without prebuilt copies (mathlib, tier1, raytrace,
rem vgui_controls, matsys_controls). +everything would also pull in SDK utility
rem tool projects (height2normal, motionmapper, ...) that require MFC but are
rem not used by the mod.
rem The $Lib references in server_base.vpc / client_base.vpc still add link
rem dependencies automatically; the +xy entries make sure the .lib producing
rem projects exist in the solution.
devtools\bin\vpc.exe /portal /define:SOURCESDK +server +client +launcher_main +mathlib +tier1 +raytrace +vgui_controls +matsys_controls /mksln portal.sln
if errorlevel 1 (
    echo.
    echo Project generation failed.
    exit /b 1
)

echo.
echo Generated src\portal.sln
endlocal
