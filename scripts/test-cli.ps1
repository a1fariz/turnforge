param(
    [string]$Executable = '',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $Executable) { $Executable = Join-Path $projectRoot 'build/Release/turnforge.exe' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $projectRoot 'build/qa-cli' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$runDirectory = Join-Path ([System.IO.Path]::GetFullPath($OutputDirectory)) ([guid]::NewGuid().ToString('N'))
[void][System.IO.Directory]::CreateDirectory($runDirectory)
$results = @()
function Test-Invocation {
    param([string]$Name, [string[]]$Arguments, [int]$ExpectedExit = 1, [string]$Binary = $Executable, [switch]$BeforeWindow)
    $stdout = Join-Path $runDirectory ($Name + '.stdout.log')
    $stderr = Join-Path $runDirectory ($Name + '.stderr.log')
    $app = Start-Process -FilePath $Binary -ArgumentList $Arguments -WorkingDirectory $runDirectory -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    try {
        if (-not $app.WaitForExit(60000)) { throw "Timed out: $Name" }
        if ($app.ExitCode -ne $ExpectedExit) { throw "$Name returned $($app.ExitCode), expected $ExpectedExit" }
        if ($BeforeWindow -and (Select-String -LiteralPath $stdout -SimpleMatch 'Initializing raylib' -Quiet)) {
            throw "$Name opened a window before rejecting invalid input"
        }
    } finally {
        if (-not $app.HasExited) { $app.Kill(); $app.WaitForExit() }
    }
    Write-Host "PASS CLI: $Name"
    return [PSCustomObject]@{Name=$Name;Passed=$true;ExitCode=$app.ExitCode}
}
foreach ($case in @(
    @{Name='unknown-option'; Arguments=@('--unknown')},
    @{Name='missing-capture-path'; Arguments=@('--capture-dir')},
    @{Name='missing-duration'; Arguments=@('--benchmark-seconds')},
    @{Name='zero-duration'; Arguments=@('--benchmark-seconds','0')},
    @{Name='negative-duration'; Arguments=@('--benchmark-seconds','-1')},
    @{Name='nan-duration'; Arguments=@('--benchmark-seconds','nan')},
    @{Name='infinite-duration'; Arguments=@('--benchmark-seconds','inf')},
    @{Name='overflow-duration'; Arguments=@('--benchmark-seconds','1e9999')},
    @{Name='excessive-duration'; Arguments=@('--benchmark-seconds','3601')},
    @{Name='trailing-number-text'; Arguments=@('--benchmark-seconds','0.1junk')},
    @{Name='nonnumeric-duration'; Arguments=@('--benchmark-seconds','text')},
    @{Name='extra-option'; Arguments=@('--smoke-test','--unknown')}
)) {
    $results += Test-Invocation -Name $case.Name -Arguments $case.Arguments -BeforeWindow
}
$blockedPath = Join-Path $runDirectory 'a-file-not-a-directory'
[System.IO.File]::WriteAllText($blockedPath, 'QA fixture')
$results += Test-Invocation -Name 'capture-path-is-file' -Arguments @('--smoke-test','--capture-dir',('"' + $blockedPath + '"')) -BeforeWindow
foreach ($fixture in @('missing-assets','corrupt-atlas','empty-atlas')) {
    $fixtureDirectory = Join-Path $runDirectory $fixture
    [void][System.IO.Directory]::CreateDirectory($fixtureDirectory)
    $fixtureBinary = Join-Path $fixtureDirectory 'turnforge.exe'
    Copy-Item -LiteralPath $Executable -Destination $fixtureBinary
    if ($fixture -ne 'missing-assets') {
        [void][System.IO.Directory]::CreateDirectory((Join-Path $fixtureDirectory 'assets'))
        $atlasPath = Join-Path $fixtureDirectory 'assets/characters.png'
        if ($fixture -eq 'corrupt-atlas') { [System.IO.File]::WriteAllText($atlasPath, 'Not a PNG') }
        else {
            Add-Type -AssemblyName System.Drawing
            $bitmap = [System.Drawing.Bitmap]::new(30, 10)
            try { $bitmap.Save($atlasPath, [System.Drawing.Imaging.ImageFormat]::Png) }
            finally { $bitmap.Dispose() }
        }
    }
    $results += Test-Invocation -Name $fixture -Binary $fixtureBinary -Arguments @('--smoke-test')
}
$captureDirectory = Join-Path $runDirectory 'short benchmark with spaces'
$results += Test-Invocation -Name 'short-benchmark-and-spaced-path' -ExpectedExit 0 -Arguments @('--benchmark-seconds','0.1','--capture-dir',('"' + $captureDirectory + '"'))
$report = Get-Content -LiteralPath (Join-Path $captureDirectory 'verification.json') -Raw | ConvertFrom-Json
if (-not $report.passed -or $report.frames -lt 360 -or -not $report.input_checks -or -not $report.window_close_check) {
    throw 'Short benchmark did not complete its required checks'
}
$summary = [PSCustomObject]@{Passed=$true;Cases=$results;ReportDirectory=$runDirectory}
$summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $runDirectory 'results.json') -Encoding utf8
Write-Output ('CLI and asset QA passed; report: ' + (Join-Path $runDirectory 'results.json'))
