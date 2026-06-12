# analyze.ps1 -- run the session analyzer over every recorded play session.
# Writes a markdown report next to each JSONL. Usage:
#   .\analyze.ps1                 (analyzes Technical\StatisticalEngine\telemetry\)
#   .\analyze.ps1 -Path <dir>     (analyze a different directory of session_*.jsonl)
param([string]$Path = (Join-Path $PSScriptRoot 'Technical\StatisticalEngine\telemetry'))
$ErrorActionPreference = 'Stop'
$analyzer = Join-Path $PSScriptRoot 'Technical\_playtest\analyze_session.py'
$sessions = Get-ChildItem -Path $Path -Filter 'session_*.jsonl' -File -ErrorAction SilentlyContinue

if (-not $sessions) {
    Write-Host "No sessions found in $Path -- play first (.\play.ps1)." -ForegroundColor Yellow
    exit 0
}
foreach ($s in $sessions) {
    $report = [System.IO.Path]::ChangeExtension($s.FullName, $null) + 'report.md'
    python $analyzer $s.FullName -o $report
    Write-Host "Analyzed $($s.Name) -> $(Split-Path $report -Leaf)" -ForegroundColor Green
}
Write-Host "`n$($sessions.Count) session(s) analyzed. Share the reports (or just say 'analyze my sessions')."
