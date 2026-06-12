# play.ps1 -- one-command STVG launch.
# Kills zombie processes, starts the game server, opens the browser, and
# shuts the server down when you close this window / press Ctrl+C.
# Usage:  .\play.ps1            (build assumed present; see README Quick Start to build)
#         .\play.ps1 -NoBrowser (start server only -- used by sanity checks)
param(
    [switch]$NoBrowser,
    [switch]$SanityExit,   # verify server comes up, then stop it and exit (for checks)
    [int]$Port = 8080
)
# Prefer PowerShell 7 — if launched under Windows PowerShell 5.1 (e.g. right-click
# "Run with PowerShell"), re-exec under pwsh when available (5.1 has shown hangs here).
if ($PSVersionTable.PSVersion.Major -lt 7) {
    $pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
    if ($pwsh) {
        $fwd = @('-NoProfile', '-File', $PSCommandPath)
        if ($NoBrowser) { $fwd += '-NoBrowser' }
        if ($SanityExit) { $fwd += '-SanityExit' }
        $fwd += @('-Port', $Port)
        & $pwsh.Source @fwd
        exit $LASTEXITCODE
    }
}

$engine = Join-Path $PSScriptRoot 'Technical\StatisticalEngine'
$exe = Join-Path $engine 'build\Debug\stvg_server.exe'

if (-not (Test-Path $exe)) {
    Write-Host "Server not built. From $engine run:" -ForegroundColor Yellow
    Write-Host '  cmake -B build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Debug -Wno-dev'
    Write-Host '  cmake --build build --config Debug --target stvg_server --parallel 4'
    exit 1
}

# Native stderr must not terminate the script under 5.1 -- route via cmd.
cmd /c "taskkill /F /IM stvg_server.exe >nul 2>&1"
cmd /c "taskkill /F /IM stvg_tests.exe >nul 2>&1"
cmd /c "taskkill /F /IM stvg_autoplay.exe >nul 2>&1"
$ErrorActionPreference = 'Stop'

$server = Start-Process -FilePath $exe -ArgumentList "$Port", 'static' `
    -WorkingDirectory $engine -WindowStyle Hidden -PassThru
Start-Sleep -Seconds 3

try {
    $null = Invoke-WebRequest -Uri "http://localhost:$Port/" -UseBasicParsing -TimeoutSec 10
    Write-Host "STVG server up at http://localhost:$Port/  (PID $($server.Id))" -ForegroundColor Green
} catch {
    Write-Host "Server failed to respond on port $Port." -ForegroundColor Red
    Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    exit 1
}

if ($SanityExit) {
    Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    Write-Host 'Sanity check passed; server stopped.' -ForegroundColor Green
    exit 0
}

if (-not $NoBrowser) { Start-Process "http://localhost:$Port/" }

Write-Host "Telemetry records to Technical\StatisticalEngine\telemetry\ -- just play."
Write-Host "Press Ctrl+C (or close this window) to stop the server."
try {
    Wait-Process -Id $server.Id
} finally {
    if (-not $server.HasExited) { Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue }
    Write-Host 'Server stopped.' -ForegroundColor Yellow
}
