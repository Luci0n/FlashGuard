@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Build modes:
rem   build.bat           -> fast development build (/Od /Ob0)
rem   build.bat fast      -> fast development build
rem   build.bat dev       -> fast development build
rem   build.bat release   -> optimized release build (/O2)

set "MODE=%~1"
if not defined MODE set "MODE=fast"

if /I "%MODE%"=="fast" (
  set "OPT=/Od /Ob0"
) else if /I "%MODE%"=="dev" (
  set "OPT=/Od /Ob0"
) else if /I "%MODE%"=="release" (
  set "OPT=/O2"
) else (
  echo ERROR: Unknown build mode "%MODE%".
  echo Usage: build.bat [fast^|dev^|release]
  exit /b 2
)

rem Always initialize the complete Visual Studio developer environment. A
rem persistent runner can retain cl.exe in PATH after the companion MSVC DLL
rem paths are gone, which makes cl fail later with missing mspdbcore.dll.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
  echo ERROR: vswhere.exe was not found.
  echo Install Visual Studio 2022 or Build Tools with Desktop development with C++.
  exit /b 2
)

for /f "usebackq tokens=*" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
if not defined VSINSTALL (
  echo ERROR: MSVC C++ build tools were not found.
  echo Add Desktop development with C++ in Visual Studio Installer.
  exit /b 2
)

call "!VSINSTALL!\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64 >nul
if errorlevel 1 (
  echo ERROR: Visual Studio build environment setup failed.
  exit /b 2
)

set "ROOT=%~dp0.."
cd /d "%ROOT%"
if not exist build mkdir build
if errorlevel 1 (
  echo ERROR: Could not create the build directory.
  exit /b 3
)

pushd build

echo Building FlashGuard [%MODE%]...
cl /nologo /std:c++20 /EHsc %OPT% /W4 /permissive- /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX ..\src\FlashGuard.cpp /Fo:FlashGuard.obj /Fe:FlashGuard.exe /link /INCREMENTAL user32.lib gdi32.lib comctl32.lib shell32.lib dwmapi.lib uxtheme.lib d3d11.lib dxgi.lib d3dcompiler.lib runtimeobject.lib windowsapp.lib
if errorlevel 1 (
  echo ERROR: Compilation failed.
  popd
  exit /b 1
)

copy /Y FlashGuard.exe ..\FlashGuard.exe >nul
if errorlevel 1 (
  echo ERROR: The build compiled, but FlashGuard.exe could not be replaced.
  echo Close every running FlashGuard instance and run build.bat again.
  popd
  exit /b 4
)

popd
echo Built "%ROOT%\FlashGuard.exe" [%MODE%]
exit /b 0
