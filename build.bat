@echo off
setlocal
rem Run from this script's own directory (portable across checkouts).
cd /d "%~dp0"

rem Locate a VS/BuildTools install with the C++ toolset via vswhere.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" ( echo VSWHERE_NOT_FOUND & exit /b 1 )
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL ( echo VS_TOOLSET_NOT_FOUND & exit /b 1 )

call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x86
if errorlevel 1 ( echo VCVARS_FAILED & exit /b 1 )

rem afxres.h is header-only resource defs. BuildTools installs have no MFC, so if
rem the resource compiler can't find it, point MFCINC at an atlmfc\include dir
rem from an MFC-equipped VS install and uncomment the two lines below.
rem set "MFCINC=C:\Path\To\VC\Tools\MSVC\<version>\atlmfc\include"
rem set "INCLUDE=%MFCINC%;%INCLUDE%"

msbuild AloViewer.sln /m /p:Configuration=Release /p:Platform=x86 /p:UseEnv=true /nologo /v:m
echo MSBUILD_EXIT=%errorlevel%
endlocal
