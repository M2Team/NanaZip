@rem Minimize.cmd - shrink a fuzzer corpus to the minimal set of inputs that
@rem preserves current coverage, and reduce each input's size.
@rem
@rem Usage:  Minimize.cmd <Format> [corpus_dir]
@rem   e.g.  Minimize.cmd Ufs
@rem         Minimize.cmd WebAssembly corpus\Wasm
@rem
@rem What it does:
@rem   1. Creates corpus\<Format>-min with the minimal covering set
@rem   2. Renames the original corpus to corpus\<Format>-pre-min
@rem   3. Renames the minimized corpus to corpus\<Format>
@rem
@rem The original corpus is kept as corpus\<Format>-pre-min in case
@rem something goes wrong. Delete it once you're satisfied.
@echo off
setlocal

if "%~1"=="" (
    echo Usage: %~nx0 ^<Format^> [corpus_dir]
    echo   Format: Ufs ^| DotNetSingleFile ^| ElectronAsar ^| Romfs ^| Zealfs ^| WebAssembly ^| Littlefs ^| Avb ^| Lvm
    exit /b 2
)

set "HARNESS=%~1"
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

if "%~2"=="" (
    set "CORPUS=%ROOT%\corpus\%HARNESS%"
) else (
    for %%I in ("%~2") do set "CORPUS=%%~fI"
)

set "EXE=%ROOT%\Fuzz.%HARNESS%.exe"
if not exist "%EXE%" (
    echo [Minimize] ERROR: %EXE% not found
    exit /b 3
)
if not exist "%CORPUS%" (
    echo [Minimize] ERROR: corpus dir not found: %CORPUS%
    exit /b 3
)

set "MIN=%CORPUS%-min"
set "BACKUP=%CORPUS%-pre-min"

if exist "%MIN%" (
    echo [Minimize] ERROR: %MIN% already exists, remove it first
    exit /b 4
)
if exist "%BACKUP%" (
    echo [Minimize] ERROR: %BACKUP% already exists, remove it first
    exit /b 4
)

set "ASAN_OPTIONS=allocator_may_return_null=1:detect_leaks=0:abort_on_error=0"

echo [Minimize] harness = %HARNESS%
echo [Minimize] corpus  = %CORPUS%
echo [Minimize] output  = %MIN%

rem Count inputs before
for /f %%N in ('dir /b /a-d "%CORPUS%" 2^>nul ^| find /c /v ""') do set "BEFORE_COUNT=%%N"
for /f "tokens=3" %%S in ('dir "%CORPUS%" 2^>nul ^| findstr /c:"File(s)"') do set "BEFORE_SIZE=%%S"
echo [Minimize] Before: %BEFORE_COUNT% files, %BEFORE_SIZE% bytes

mkdir "%MIN%"

echo [Minimize] Running -merge=1 ...
"%EXE%" -merge=1 "%MIN%" "%CORPUS%"
if errorlevel 1 (
    echo [Minimize] ERROR: merge failed ^(exit code %ERRORLEVEL%^)
    rmdir /s /q "%MIN%" 2>nul
    exit /b 5
)

rem Count inputs after
for /f %%N in ('dir /b /a-d "%MIN%" 2^>nul ^| find /c /v ""') do set "AFTER_COUNT=%%N"
for /f "tokens=3" %%S in ('dir "%MIN%" 2^>nul ^| findstr /c:"File(s)"') do set "AFTER_SIZE=%%S"
echo [Minimize] After:  %AFTER_COUNT% files, %AFTER_SIZE% bytes

rem Swap directories
echo [Minimize] Renaming %CORPUS% -^> %BACKUP%
ren "%CORPUS%" "%HARNESS%-pre-min"
echo [Minimize] Renaming %MIN% -^> %CORPUS%
ren "%MIN%" "%HARNESS%"

echo [Minimize] Done. Original corpus saved as %BACKUP%
echo [Minimize] To discard the original: rmdir /s /q "%BACKUP%"

endlocal
