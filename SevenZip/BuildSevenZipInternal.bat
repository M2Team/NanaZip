@setlocal EnableDelayedExpansion
@echo off

set VisualStudioInstallerFolder="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if %PROCESSOR_ARCHITECTURE%==x86 set VisualStudioInstallerFolder="%ProgramFiles%\Microsoft Visual Studio\Installer"

pushd %VisualStudioInstallerFolder%
for /f "usebackq tokens=*" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set VisualStudioInstallDir=%%i
)
popd

if %1==x86 set VisualStudioToolKitType=amd64_x86
if %1==x64 set VisualStudioToolKitType=amd64
if %1==ARM set VisualStudioToolKitType=amd64_arm
if %1==ARM64  set VisualStudioToolKitType=amd64_arm64

call "%VisualStudioInstallDir%\VC\Auxiliary\Build\vcvarsall.bat" %VisualStudioToolKitType%

call "%~dp0VC-LTL.cmd"

pushd "%~dp0CPP\7zip"
nmake
popd

@endlocal
