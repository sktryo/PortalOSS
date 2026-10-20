@echo off
rem Build the Portal mod through MSBuild without the Visual Studio IDE noise.
rem
rem Usage:
rem   buildportal.bat                  build Server (Portal), Release
rem   buildportal.bat server           same
rem   buildportal.bat client           build Client (Portal)
rem   buildportal.bat launcher         build the launcher exe (post-build copies it)
rem   buildportal.bat all              build the whole solution
rem   buildportal.bat server rebuild   clean + build that project
rem   buildportal.bat server debug     use the Debug configuration
rem
rem Only errors and the summary are printed, so SDK warning spam and the
rem per-file VPC macro noise do not bury the real problem. Run it from the
rem src directory (or anywhere; it cd's to its own directory).
setlocal EnableExtensions
cd /d "%~dp0"

set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=server"
set "ACTION=%~2"
if "%ACTION%"=="" set "ACTION=build"
set "CONFIG=%~3"
if "%CONFIG%"=="" set "CONFIG=Release"

if /i "%ACTION%"=="release" set "CONFIG=Release"
if /i "%ACTION%"=="debug"   set "CONFIG=Debug"

if /i "%ACTION%"=="rebuild" goto :config_given
if /i "%ACTION%"=="build"   goto :config_given
rem Second argument was really the configuration
set "CONFIG=%ACTION%"
set "ACTION=build"
:config_given

if not exist "portal.sln" (
	echo [buildportal] portal.sln not found, running createportalprojects.bat...
	call createportalprojects.bat
	if errorlevel 1 (
		echo [buildportal] project generation failed.
		exit /b 1
	)
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
if exist "%VSWHERE%" (
	for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VSPATH=%%i"
)
if not defined VSPATH (
	echo [buildportal] Visual Studio with the C++ workload was not found.
	exit /b 1
)

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
	echo [buildportal] vcvars64.bat failed.
	exit /b 1
)

rem ---------------------------------------------------------------------
rem VPC on Windows x64 generates the solution configurations
rem "<Config>|win64" (Debug|win64, Release|win64). MSBuild rejects any
rem Configuration/Platform pair the solution does not list, so default the
rem platform to win64; the 4th argument overrides it for other generators.
rem ---------------------------------------------------------------------
set "SOLCONFIG=%CONFIG%"
set "SOLPLATFORM=%~4"
if not defined SOLPLATFORM set "SOLPLATFORM=win64"
set "CFGARGS=/p:Configuration=%SOLCONFIG% /p:Platform=%SOLPLATFORM%"
echo [buildportal] solution configuration: %SOLCONFIG%^|%SOLPLATFORM%

rem Warn (but continue) when the solution does not list the pair.
findstr /i /c:"%SOLCONFIG%|%SOLPLATFORM% =" portal.sln >nul 2>&1
if errorlevel 1 (
	echo [buildportal] warning: "%SOLCONFIG%|%SOLPLATFORM%" is not listed in portal.sln.
	echo [buildportal] Listed configurations:
	findstr /i /c:" = " portal.sln
	echo [buildportal] Override with: buildportal.bat %TARGET% %ACTION% %SOLCONFIG% ^<platform^>
)

if /i "%TARGET%"=="server" (
	set "PROJECT=Server (Portal)"
	goto :have_target
)
if /i "%TARGET%"=="client" (
	set "PROJECT=Client (Portal)"
	goto :have_target
)
if /i "%TARGET%"=="launcher" (
	set "PROJECT=launcher_main"
	goto :have_target
)
if /i "%TARGET%"=="all" goto :build_all

echo [buildportal] unknown target '%TARGET%' ^(use server^|client^|launcher^|all^)
exit /b 1

:have_target
if /i "%ACTION%"=="rebuild" (
	echo [buildportal] Rebuild %PROJECT% ^(%CONFIG%^)
	msbuild portal.sln /m /t:"%PROJECT%:Rebuild" %CFGARGS% /v:m /clp:ErrorsOnly;Summary
) else (
	echo [buildportal] Build %PROJECT% ^(%CONFIG%^)
	msbuild portal.sln /m /t:"%PROJECT%" %CFGARGS% /v:m /clp:ErrorsOnly;Summary
)
exit /b %errorlevel%

:build_all
if /i "%ACTION%"=="rebuild" (
	echo [buildportal] Rebuild solution ^(%CONFIG%^)
	msbuild portal.sln /m /t:Rebuild %CFGARGS% /v:m /clp:ErrorsOnly;Summary
) else (
	echo [buildportal] Build solution ^(%CONFIG%^)
	msbuild portal.sln /m /t:Build %CFGARGS% /v:m /clp:ErrorsOnly;Summary
)
exit /b %errorlevel%
