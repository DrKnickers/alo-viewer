@echo off
cd /d C:\Modding\alo-viewer-investigate
rem VS2022 BuildTools compiler (14.44 == v143-compatible, working CL).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86
if errorlevel 1 ( echo VCVARS_FAILED & exit /b 1 )
rem afxres.h is header-only resource defs; BuildTools has no MFC, borrow it from VS18 14.31.
set MFCINC=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.31.31103\atlmfc\include
set INCLUDE=%MFCINC%;%INCLUDE%
msbuild AloViewer.sln /m /p:Configuration=Release /p:Platform=x86 /p:UseEnv=true /nologo /v:m
echo MSBUILD_EXIT=%errorlevel%
