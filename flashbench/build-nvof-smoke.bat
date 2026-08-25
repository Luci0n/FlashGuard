@echo off
setlocal EnableExtensions EnableDelayedExpansion

where cl.exe >nul 2>nul
if errorlevel 1 (
  set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  if not exist "!VSWHERE!" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
  if not exist "!VSWHERE!" (
    echo ERROR: vswhere.exe was not found.
    exit /b 2
  )
  for /f "usebackq tokens=*" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
  if not defined VSINSTALL (
    echo ERROR: MSVC C++ build tools were not found.
    exit /b 2
  )
  call "!VSINSTALL!\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64 >nul
  if errorlevel 1 exit /b 2
)

set "ROOT=%~dp0.."
cd /d "%ROOT%"
if not exist build mkdir build

echo Building NVOFA smoke helper...
cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX flashbench\NvofSmoke.cpp /Fo:build\NvofSmoke.obj /Fe:build\NvofSmoke.exe /link d3d11.lib dxgi.lib
if errorlevel 1 exit /b 1

echo Built "%ROOT%\build\NvofSmoke.exe"
exit /b 0
