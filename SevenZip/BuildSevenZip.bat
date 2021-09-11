@setlocal
@echo off

rem Set the code page to reduce the compilation warnings
chcp 65001

call "%~dp0BuildSevenZipInternal.bat" x86
call "%~dp0BuildSevenZipInternal.bat" x64
call "%~dp0BuildSevenZipInternal.bat" ARM
call "%~dp0BuildSevenZipInternal.bat" ARM64

@endlocal
