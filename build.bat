@echo off
rem Compile BOURDONNEMENT avec MSVC (trouve automatiquement via vswhere)
cd /d "%~dp0"
where cl >nul 2>nul
if not errorlevel 1 goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\vspath.txt"
set /p VSPATH=<"%TEMP%\vspath.txt"
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
:build
rem BOURDON_BUILD : numero de build donne par la CI (mise a jour automatique) ; absent = 0 (dev)
set "BBUILD="
if defined BOURDON_BUILD set "BBUILD=/DBOURDON_BUILD=%BOURDON_BUILD%"
cl /nologo /O2 /W3 /std:c11 %BBUILD% /Fe:bourdonnement.exe main.c /link /SUBSYSTEM:WINDOWS
