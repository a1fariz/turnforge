param(
    [string]$Executable = '',
    [string]$OutputDirectory = '',
    [ValidateRange(0,3600)][int]$BenchmarkSeconds = 0
)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $Executable) { $Executable = Join-Path $projectRoot 'build\Release\turnforge.exe' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $projectRoot 'build\verification' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
[void][System.IO.Directory]::CreateDirectory($OutputDirectory)
$arguments = @('--smoke-test','--capture-dir',('"' + $OutputDirectory + '"'))
if ($BenchmarkSeconds -gt 0) { $arguments += @('--benchmark-seconds',[string]$BenchmarkSeconds) }
$app = Start-Process -FilePath $Executable -ArgumentList $arguments -WorkingDirectory ([System.IO.Path]::GetDirectoryName($Executable)) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $OutputDirectory 'stdout.log') -RedirectStandardError (Join-Path $OutputDirectory 'stderr.log')
$clock = [System.Diagnostics.Stopwatch]::StartNew()
$samples = @()
$nextReport = 30
try {
    while (-not $app.HasExited) {
        $app.Refresh()
        if (-not $app.HasExited) {
            $samples += [PSCustomObject]@{Seconds=[math]::Round($clock.Elapsed.TotalSeconds,2);WorkingSetBytes=$app.WorkingSet64;PrivateBytes=$app.PrivateMemorySize64;PeakWorkingSetBytes=$app.PeakWorkingSet64}
        }
        if ($clock.Elapsed.TotalSeconds -gt ([math]::Max(30,$BenchmarkSeconds) + 60)) { throw 'Verification timed out.' }
        if ($BenchmarkSeconds -gt 0 -and $clock.Elapsed.TotalSeconds -ge $nextReport) {
            Write-Output ('Benchmark running: ' + [int]$clock.Elapsed.TotalSeconds + ' seconds')
            $nextReport += 30
        }
        Start-Sleep -Milliseconds 500
    }
    $app.WaitForExit()
    if ($app.ExitCode -ne 0) { throw ('Game verification failed with exit code ' + $app.ExitCode + '. See stdout.log and stderr.log.') }
} finally {
    if (-not $app.HasExited) { $app.Kill(); $app.WaitForExit() }
}
$reportPath = Join-Path $OutputDirectory 'verification.json'
if (-not (Test-Path -LiteralPath $reportPath)) { throw 'No verification report was written.' }
$report = [System.IO.File]::ReadAllText($reportPath) | ConvertFrom-Json
if (-not $report.passed) { throw 'The game reported a failed verification.' }
foreach ($field in @('character_animation_checks','input_checks','window_close_check','verified_win','verified_loss','verified_guard','verified_preview')) {
    if ($report.$field -ne $true) { throw ('Required verification did not pass: ' + $field) }
}
if ($report.verified_restarts -lt 10) { throw 'Fewer than ten restarts were verified.' }
Add-Type -AssemblyName System.Drawing
foreach ($name in @('01-title','02-gameplay','03-enemy-turn','04-victory','05-defeat','06-restart','07-character-walk','08-character-attack','09-guard','10-guard-hit','11-enemy-preview')) {
    $screenshotPath = Join-Path $OutputDirectory ($name + '.png')
    if (-not (Test-Path -LiteralPath $screenshotPath)) { throw ('Screenshot missing: ' + $name) }
    $screenshot = [System.Drawing.Image]::FromFile($screenshotPath)
    try {
        if ($screenshot.Width -ne 1280 -or $screenshot.Height -ne 720) { throw ('Unexpected screenshot dimensions: ' + $name) }
    } finally { $screenshot.Dispose() }
}
$maxWorkingSet = ($samples | Measure-Object -Property PeakWorkingSetBytes -Maximum).Maximum
$maxPrivate = ($samples | Measure-Object -Property PrivateBytes -Maximum).Maximum
$memoryReport = [PSCustomObject]@{Samples=$samples.Count;PeakWorkingSetMB=[math]::Round($maxWorkingSet/1MB,2);PeakPrivateMB=[math]::Round($maxPrivate/1MB,2);SamplingSeconds=0.5;RunMode='Hidden window; automated gameplay and reset scenarios'}
$memoryReport | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'memory.json') -Encoding utf8
$samples | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'memory-samples.json') -Encoding utf8
$report | ConvertTo-Json
$memoryReport | ConvertTo-Json
