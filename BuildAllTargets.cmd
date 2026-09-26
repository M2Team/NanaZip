@setlocal
@echo off

rem Change to the current folder.
cd "%~dp0"

rem Remove the output folder for a fresh compile.
rd /s /q Output
md Output

rem Initialize Visual Studio environment
set VisualStudioInstallerFolder="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if %PROCESSOR_ARCHITECTURE%==x86 set VisualStudioInstallerFolder="%ProgramFiles%\Microsoft Visual Studio\Installer"
pushd %VisualStudioInstallerFolder%
for /f "usebackq tokens=*" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set VisualStudioInstallDir=%%i
)
popd
call "%VisualStudioInstallDir%\VC\Auxiliary\Build\vcvarsall.bat" x86

rem Build all targets
for %%C in (Debug Release) do (
  for %%P in (x64 ARM64) do (
    MSBuild -binaryLogger:Output\BuildAllTargets_%%C_%%P.binlog -m -p:Configuration=%%C -p:Platform=%%P BuildAllTargets.proj
    if errorlevel 1 exit /b 1
  )
)

@endlocal
