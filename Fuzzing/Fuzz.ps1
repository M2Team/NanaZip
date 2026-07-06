# Fuzz.ps1 - run a NanaZip codec harness, never stop on crash.
# Usage:  pwsh Fuzz.ps1 -Format <Format> [-CorpusDir path] [-TotalSeconds N] [-NoPrintAsan]
#   e.g.  pwsh Fuzz.ps1 -Format Zealfs
#         pwsh Fuzz.ps1 -Format WebAssembly -CorpusDir corpus\Wasm
#         pwsh Fuzz.ps1 -Format Ufs -TotalSeconds 600 -NoPrintAsan
#
# Artifacts are deduplicated by content SHA-1 and written to
#   crashes\<Format>\crash-<sha1>          - crashing inputs
#   crashes\<Format>\oom-<sha1>            - out-of-memory inputs
#   crashes\<Format>\timeout-<sha1>        - >timeout-second inputs

param(
    [Parameter(Mandatory)][string]$Format,
    [string]$CorpusDir,
    [int]$TotalSeconds = 0,
    [switch]$NoPrintAsan
)

$ErrorActionPreference = 'Stop'

# When deployed, the script sits alongside the exe/corpus/dict in the output dir.
$Root = $PSScriptRoot

$Exe = Join-Path $Root "Fuzz.$Format.exe"
if (-not (Test-Path $Exe)) {
    Write-Error "[Fuzz] ERROR: $Exe not found"
    exit 3
}

if ($CorpusDir) {
    $Corpus = Resolve-Path $CorpusDir -ErrorAction Stop
} else {
    $Corpus = Join-Path $Root "corpus/$Format"
}
if (-not (Test-Path $Corpus)) {
    Write-Error "[Fuzz] ERROR: corpus dir not found: $Corpus"
    exit 3
}

$CrashDir = Join-Path $Root "crashes/$Format"
New-Item -ItemType Directory -Path $CrashDir -Force | Out-Null

# Per-harness working dir so concurrent invocations don't fight over
# fuzz-0.log / fuzz-1.log (libFuzzer fork-mode names, hardcoded).
$WorkDir = Join-Path $Root "work/$Format"
New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null

# --- Per-format dictionary ---
$dictPath = Join-Path $Root "dict/$Format.dict"
$dictArg = if (Test-Path $dictPath) { "-dict=$dictPath" } else { $null }

# --- Per-format extra libFuzzer flags ---
$extraArgs = switch ($Format) {
    'Ufs'              { @('-max_len=131072') }
    'DotNetSingleFile' { @('-max_len=262144', '-malloc_limit_mb=16') }
    'WebAssembly'      { @('-max_len=65536', '-malloc_limit_mb=16') }
    'Romfs'            { @('-malloc_limit_mb=16') }
    'Lvm'              { @('-malloc_limit_mb=16') }
    'Avb'              { @('-malloc_limit_mb=16') }
    default            { @() }
}

# --- ASan options ---
# log_path must use forward slashes and be relative (no drive letter colon,
# which ASan interprets as option separator). CWD will be work/<Format>,
# crashes are at crashes/<Format>, so relative path is ../../crashes/<Format>/asan.
# Always set here because we control the CWD via Set-Location below.
$env:ASAN_OPTIONS = "allocator_may_return_null=1:detect_leaks=0:abort_on_error=0:log_path=../../crashes/$Format/asan"

$printAsan = -not $NoPrintAsan

Write-Host "[Fuzz] harness=$Format"
Write-Host "[Fuzz] corpus =$Corpus"
Write-Host "[Fuzz] crashes=$CrashDir\"
Write-Host "[Fuzz] cwd    =$WorkDir  (fuzz-N.log lives here)"

Set-Location $WorkDir

# --- Time limit ---
$deadline = $null
if ($TotalSeconds -gt 0) {
    $deadline = [DateTimeOffset]::UtcNow.AddSeconds($TotalSeconds)
}

# Track which ASan log files we've already printed.
$printedAsan = @{}

# --- Fuzzing loop ---
while ($true) {
    # Check time limit
    if ($deadline) {
        $remaining = [int]($deadline - [DateTimeOffset]::UtcNow).TotalSeconds
        if ($remaining -le 0) {
            Write-Host "[Fuzz] Time limit reached, stopping."
            break
        }
        Write-Host "[Fuzz] ${remaining}s remaining..."
    }

    # Build argument list
    $fuzzArgs = @(
        '-fork=2'
        '-ignore_crashes=1'
        '-ignore_ooms=1'
        '-ignore_timeouts=1'
        '-timeout=30'
        '-rss_limit_mb=4096'
        '-malloc_limit_mb=256'
        '-reload=30'
        '-print_final_stats=1'
        "-artifact_prefix=$CrashDir\"
    )
    if ($dictArg) { $fuzzArgs += $dictArg }
    $fuzzArgs += $extraArgs
    if ($deadline) {
        $fuzzArgs += "-max_total_time=$remaining"
    }
    $fuzzArgs += $Corpus

    Write-Host "[Fuzz] Starting fuzzer (Ctrl-C to stop)..."
    $proc = Start-Process -FilePath $Exe -ArgumentList $fuzzArgs `
        -NoNewWindow -PassThru

    # Poll for new ASan logs while the fuzzer runs
    while (-not $proc.HasExited) {
        Start-Sleep -Seconds 5
        if ($printAsan) {
            Get-ChildItem $CrashDir -Filter 'asan.*' -File -ErrorAction SilentlyContinue |
                Where-Object { -not $printedAsan.ContainsKey($_.Name) -and $_.Length -gt 0 } |
                ForEach-Object {
                    Start-Sleep -Milliseconds 500  # let ASan finish writing
                    $content = Get-Content $_.FullName -Raw
                    $printedAsan[$_.Name] = $true
                    # Skip libFuzzer HandleMalloc traces (malloc-limit OOMs, not real bugs)
                    if ($content -match 'ERROR: AddressSanitizer|SUMMARY: AddressSanitizer') {
                        Write-Host "`n[Fuzz] === ASan report: $($_.Name) ==="
                        Write-Host $content
                        Write-Host "[Fuzz] === end ASan report ==="
                    }
                }
        }
    }
    $proc.WaitForExit()  # ensure exit code is populated

    # Print any ASan logs written between the last poll and process exit
    if ($printAsan) {
        Get-ChildItem $CrashDir -Filter 'asan.*' -File -ErrorAction SilentlyContinue |
            Where-Object { -not $printedAsan.ContainsKey($_.Name) -and $_.Length -gt 0 } |
            ForEach-Object {
                $content = Get-Content $_.FullName -Raw
                $printedAsan[$_.Name] = $true
                if ($content -match 'ERROR: AddressSanitizer|SUMMARY: AddressSanitizer') {
                    Write-Host "`n[Fuzz] === ASan report: $($_.Name) ==="
                    Write-Host $content
                    Write-Host "[Fuzz] === end ASan report ==="
                }
            }
    }

    # Check time limit after fuzzer exits
    if ($deadline) {
        $remaining = [int]($deadline - [DateTimeOffset]::UtcNow).TotalSeconds
        if ($remaining -le 0) {
            Write-Host "[Fuzz] Time limit reached, stopping."
            break
        }
    }

    Write-Host "[Fuzz] Fuzzer exited with code $($proc.ExitCode), restarting in 2s..."
    Start-Sleep -Seconds 2
}
