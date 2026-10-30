param(
    [string]$Executable = '',
    [string]$OutputFile = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $Executable) { $Executable = Join-Path $projectRoot 'build/Release/turnforge.exe' }
if (-not $OutputFile) { $OutputFile = Join-Path $projectRoot 'dist/Turnforge-Demo.mp4' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$OutputFile = [System.IO.Path]::GetFullPath($OutputFile)
[void][System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($OutputFile))
$ffmpegCommand = Get-Command ffmpeg -ErrorAction SilentlyContinue
$ffmpeg = if ($ffmpegCommand) { $ffmpegCommand.Source } else { Join-Path $projectRoot '.tools/imageio-ffmpeg-0.7.0/imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v7.1.exe' }
if (-not (Test-Path -LiteralPath $ffmpeg)) {
    $toolDirectory = Join-Path $projectRoot '.tools/imageio-ffmpeg-0.7.0'
    [void][System.IO.Directory]::CreateDirectory($toolDirectory)
    $archive = Join-Path $toolDirectory 'imageio_ffmpeg-0.7.0-py3-none-win_amd64.whl'
    Invoke-WebRequest -Uri 'https://files.pythonhosted.org/packages/e5/55/1d50cdd78d862a30c2ce44f01cea387f77b364b21fa2fecf1590258b215b/imageio_ffmpeg-0.7.0-py3-none-win_amd64.whl' -OutFile $archive
    if ((Get-FileHash -LiteralPath $archive).Hash -ne '7A22B45F60321D4172BDF2BC61F716BCE4C81B85727BD59C7C7E371055A6A440') {
        throw 'FFmpeg tool archive checksum mismatch.'
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory($archive, $toolDirectory)
}
$runDirectory = Join-Path $projectRoot ('build/demo/' + [guid]::NewGuid().ToString('N'))
[void][System.IO.Directory]::CreateDirectory($runDirectory)
$app = Start-Process -FilePath $Executable -ArgumentList @('--demo-dir',('"' + $runDirectory + '"')) -WorkingDirectory ([System.IO.Path]::GetDirectoryName($Executable)) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $runDirectory 'capture.stdout.log') -RedirectStandardError (Join-Path $runDirectory 'capture.stderr.log')
$timer = [System.Diagnostics.Stopwatch]::StartNew()
$nextReport = 30
try {
    while (-not $app.HasExited) {
        if ($timer.Elapsed.TotalSeconds -gt 600) { throw 'Demo capture timed out.' }
        if ($timer.Elapsed.TotalSeconds -ge $nextReport) {
            Write-Output ('Capturing demo: ' + [int]$timer.Elapsed.TotalSeconds + ' seconds elapsed')
            $nextReport += 30
        }
        Start-Sleep -Milliseconds 500
        $app.Refresh()
    }
    $app.WaitForExit()
    if ($app.ExitCode -ne 0) { throw ('Demo gameplay verification failed. See ' + $runDirectory) }
} finally {
    if (-not $app.HasExited) { $app.Kill(); $app.WaitForExit() }
}
$captureReport = Get-Content -LiteralPath (Join-Path $runDirectory 'demo-report.json') -Raw | ConvertFrom-Json
if (-not $captureReport.passed -or $captureReport.frames -ne 1980 -or -not $captureReport.won -or -not $captureReport.guard_reduced_actual_hit) {
    throw 'Demo did not complete the verified guarded victory.'
}
$frames = @(Get-ChildItem -LiteralPath $runDirectory -Filter 'frame-*.png' -File)
if ($frames.Count -ne 1980) { throw 'Unexpected number of rendered demo frames.' }
Write-Output 'Encoding 66-second demo...'
& $ffmpeg -hide_banner -loglevel error -y -framerate 30 -start_number 0 -i (Join-Path $runDirectory 'frame-%06d.png') -frames:v 1980 -c:v libx264 -preset fast -crf 20 -pix_fmt yuv420p -movflags +faststart -an $OutputFile
if ($LASTEXITCODE -ne 0) { throw 'Demo encoding failed.' }
$decodeLog = Join-Path $runDirectory 'decode.log'
$progressFile = Join-Path $runDirectory 'decode-progress.txt'
& $ffmpeg -hide_banner -nostats -i $OutputFile -map 0:v:0 -f null - -progress $progressFile 2> $decodeLog
if ($LASTEXITCODE -ne 0) { throw 'Encoded demo failed full decode verification.' }
$decodedFrames = @((Get-Content -LiteralPath $progressFile) | Where-Object { $_ -match '^frame=' })[-1]
if ($decodedFrames -ne 'frame=1980') { throw 'Decoded video frame count differs from capture.' }
$metadata = Get-Content -LiteralPath $decodeLog -Raw
if ($metadata -notmatch '1280x720' -or $metadata -notmatch '30 fps' -or $metadata -notmatch 'Duration: 00:01:06\.00' -or $metadata -notmatch 'Video: h264') {
    throw 'Encoded demo format does not match 1280x720 H.264 / 30 FPS / 66 seconds.'
}
$captureReport | Add-Member -NotePropertyName 'video_codec' -NotePropertyValue 'H.264 / yuv420p'
$captureReport | Add-Member -NotePropertyName 'width' -NotePropertyValue 1280
$captureReport | Add-Member -NotePropertyName 'height' -NotePropertyValue 720
$captureReport | Add-Member -NotePropertyName 'fully_decoded_frames' -NotePropertyValue 1980
$captureReport | Add-Member -NotePropertyName 'video_sha256' -NotePropertyValue (Get-FileHash -LiteralPath $OutputFile).Hash
$captureReport | Add-Member -NotePropertyName 'video_bytes' -NotePropertyValue (Get-Item -LiteralPath $OutputFile).Length
$captureReport | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runDirectory 'verification.json') -Encoding utf8
Write-Output ('Demo verified: ' + $OutputFile)
Write-Output ('Capture and verification evidence: ' + $runDirectory)
