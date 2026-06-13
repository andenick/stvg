# pull_telemetry.ps1 -- pull STVG gameplay telemetry from the Carson box to the dev PC.
# =====================================================================================
# WHAT IT DOES
#   Streams the box container's telemetry tree (/var/lib/stvg/telemetry, per-day
#   YYYY-MM-DD dirs of session_*.jsonl) to this PC, preserving the directory
#   structure, into:
#       Technical\StatisticalEngine\telemetry_remote\YYYY-MM-DD\session_*.jsonl
#   This dir is kept SEPARATE from local playtest telemetry
#   (Technical\StatisticalEngine\telemetry\) so box and local sessions never mix.
#
# MECHANISM (chosen for robustness on this Windows box -- see notes below):
#   ssh <box> "docker exec stvg-web tar -cz -C /var/lib/stvg telemetry"  | tar -xz ...
#   i.e. a tar-over-SSH stream read from INSIDE the container, extracted locally
#   with GNU tar (tar 1.35 ships with Windows 10/11). Rationale:
#     * `docker exec ... tar` reads from inside the container, so it needs NO sudo
#       and NO `docker volume inspect` host-path lookup (the volume Mountpoint under
#       /var/lib/docker is root-owned and not readable without sudo on the box).
#     * A single compressed stream is faster + more reliable than `scp -r` over many
#       tiny JSONL files, and needs no rsync (not installed on this dev PC).
#     * GNU tar handles the gzip + extraction natively on Windows; no WSL required.
#   Fallbacks if you ever need them are documented at the bottom of this header.
#
# ENCODING TRAP (PowerShell 5.1): this file is ASCII-only on purpose. PowerShell 5.1
#   mis-encodes non-ASCII bytes in here-strings / piped binary unless the file is
#   ASCII/UTF-8-no-BOM (same lesson as play.ps1). Do not add box-drawing chars,
#   smart quotes, or emoji to this script.
#
# USAGE
#   .\pull_telemetry.ps1                 # pull all days into telemetry_remote\
#   .\pull_telemetry.ps1 -Analyze        # pull, then run analyze_session.py on each
#                                        #   pulled session_*.jsonl (writes *report.md)
#   .\pull_telemetry.ps1 -Host 100.99.131.74   # pull over Tailscale instead of LAN
#
# REQUIREMENTS: OpenSSH client (ssh.exe) + GNU tar (tar.exe) on PATH (both ship with
#   Windows 10/11); the SSH key at C:\Users\anden\.ssh\id_ed25519; the stvg-web
#   container running on the box.
#
# FALLBACKS (manual, if the container path ever changes):
#   (A) scp the per-day dir:  scp -i <key> -r andenick@<host>:<BOXVOL>/telemetry/<day> <dest>
#       where <BOXVOL> = (ssh box) docker volume inspect stvg_stvg_data -f '{{.Mountpoint}}'
#       -- but that path is root-owned; needs sudo, which the box does not grant.
#   (B) box-side staging:  ssh box "docker cp stvg-web:/var/lib/stvg/telemetry ~/stvg-tel-export"
#       then scp -r ~/stvg-tel-export <dest>.  The tar-stream below collapses both steps.
# =====================================================================================

[CmdletBinding()]
param(
    [string]$BoxHost = '192.168.0.174',
    [string]$KeyPath = 'C:\Users\anden\.ssh\id_ed25519',
    [string]$Container = 'stvg-web',
    [switch]$Analyze
)

$ErrorActionPreference = 'Stop'

# Resolve destination relative to this script: Technical\StatisticalEngine\telemetry_remote\
$root = $PSScriptRoot
$dest = Join-Path $root 'Technical\StatisticalEngine\telemetry_remote'
if (-not (Test-Path $dest)) { New-Item -ItemType Directory -Path $dest -Force | Out-Null }

$ssh = (Get-Command ssh.exe -ErrorAction SilentlyContinue)
$tar = (Get-Command tar.exe -ErrorAction SilentlyContinue)
if (-not $ssh) { Write-Host 'ERROR: ssh.exe not found on PATH.' -ForegroundColor Red; exit 1 }
if (-not $tar) { Write-Host 'ERROR: tar.exe (GNU tar) not found on PATH.' -ForegroundColor Red; exit 1 }

Write-Host "Pulling STVG telemetry from $BoxHost ($Container) -> $dest" -ForegroundColor Cyan

# Stream the container's telemetry tree as gzip'd tar over SSH, extract locally.
# -C /var/lib/stvg + 'telemetry' => archive members begin with 'telemetry/...', so
# extracting under $dest's PARENT would nest; instead we strip the leading 'telemetry/'
# component and extract its CONTENTS (the YYYY-MM-DD day dirs) straight into $dest.
$remoteCmd = "docker exec $Container tar -cz -C /var/lib/stvg telemetry 2>/dev/null"

# Build the SSH stream and pipe its bytes to GNU tar. We use a temp file for the
# stream to avoid any PowerShell pipeline byte-mangling of the gzip stream (5.1 will
# corrupt binary through a string pipeline); a temp file is the safe, portable path.
$tmp = Join-Path $env:TEMP ("stvg_tel_{0}.tgz" -f ([guid]::NewGuid().ToString('N')))
try {
    # Redirect ssh stdout (binary gzip) straight to the temp file via cmd to keep
    # PowerShell out of the byte stream entirely.
    & cmd /c "ssh -i `"$KeyPath`" andenick@$BoxHost `"$remoteCmd`" > `"$tmp`""
    if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: ssh/docker exec exited $LASTEXITCODE." -ForegroundColor Red; exit 1 }
    if (-not (Test-Path $tmp) -or (Get-Item $tmp).Length -eq 0) {
        Write-Host 'No telemetry returned (empty stream) -- nothing to pull yet.' -ForegroundColor Yellow
        exit 0
    }
    # Extract: strip the leading 'telemetry/' so day-dirs land directly in $dest.
    # GNU tar on Windows is happiest with FORWARD-slash paths and --force-local
    # (otherwise a leading drive letter "C:\..." is read as a remote host:path and
    # backslashes get mangled). Convert both the archive (-f) and chdir (-C) paths.
    $tmpFwd  = $tmp.Replace('\','/')
    $destFwd = $dest.Replace('\','/')
    & $tar.Source --force-local -xz --strip-components=1 -f $tmpFwd -C $destFwd
    if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: tar extract exited $LASTEXITCODE." -ForegroundColor Red; exit 1 }
}
finally {
    if (Test-Path $tmp) { Remove-Item $tmp -Force -ErrorAction SilentlyContinue }
}

$pulled = Get-ChildItem -Path $dest -Recurse -Filter 'session_*.jsonl' -File -ErrorAction SilentlyContinue
Write-Host ("Pulled {0} session file(s) into {1}" -f $pulled.Count, $dest) -ForegroundColor Green
$pulled | ForEach-Object { Write-Host ("  {0}" -f $_.FullName.Substring($dest.Length).TrimStart('\')) }

if ($Analyze) {
    $analyzer = Join-Path $root 'Technical\_playtest\analyze_session.py'
    if (-not (Test-Path $analyzer)) { Write-Host "Analyzer not found: $analyzer" -ForegroundColor Yellow; exit 0 }
    foreach ($s in $pulled) {
        $report = [System.IO.Path]::ChangeExtension($s.FullName, $null) + 'report.md'
        python $analyzer $s.FullName -o $report
        if ($LASTEXITCODE -eq 0) { Write-Host ("  analyzed {0} -> {1}" -f $s.Name, (Split-Path $report -Leaf)) -ForegroundColor Green }
    }
}
