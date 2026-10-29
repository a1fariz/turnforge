param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [switch]$Package
)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$compiler = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $compiler -or -not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    throw 'Install MinGW-w64 and put its bin directory (g++ and gcc) on PATH, then run this script again.'
}
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmakeExe = if ($cmakeCommand) { $cmakeCommand.Source } else { Join-Path $projectRoot '.tools\cmake-3.31.6-windows-x86_64\bin\cmake.exe' }
if (-not (Test-Path -LiteralPath $cmakeExe)) {
    $toolsDirectory = Join-Path $projectRoot '.tools'
    [void][System.IO.Directory]::CreateDirectory($toolsDirectory)
    $archive = Join-Path $toolsDirectory 'cmake-3.31.6.zip'
    Write-Output 'Downloading project-local CMake 3.31.6...'
    Invoke-WebRequest -Uri 'https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-windows-x86_64.zip' -OutFile $archive
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne 'D163CD3AB4959B0A53FA8988F2DDBD2E6C501658201E6A154386BAD9DBE4F836') {
        throw 'CMake archive checksum mismatch; extraction stopped.'
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $toolsDirectory -Force
}
$buildDirectory = Join-Path $projectRoot ('build\' + $Configuration)
$generator = if (Get-Command ninja -ErrorAction SilentlyContinue) { 'Ninja' } else { 'MinGW Makefiles' }
$configure = @('-S',$projectRoot,'-B',$buildDirectory,'-G',$generator,('-DCMAKE_BUILD_TYPE=' + $Configuration),'-DCMAKE_C_COMPILER=gcc','-DCMAKE_CXX_COMPILER=g++')
$localRaylib = Join-Path $projectRoot '.deps\raylib-5.5'
if (Test-Path -LiteralPath (Join-Path $localRaylib 'CMakeLists.txt')) { $configure += '-DFETCHCONTENT_SOURCE_DIR_RAYLIB=' + $localRaylib }
& $cmakeExe @configure
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmakeExe --build $buildDirectory --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
$ctestExe = Join-Path ([System.IO.Path]::GetDirectoryName($cmakeExe)) 'ctest.exe'
& $ctestExe --test-dir $buildDirectory --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Gameplay tests failed.' }
if ($Package) {
    $packageDirectory = Join-Path $projectRoot 'dist\Turnforge'
    & $cmakeExe --install $buildDirectory --prefix $packageDirectory --component Turnforge
    if ($LASTEXITCODE -ne 0) { throw 'Package installation failed.' }
    Compress-Archive -Path (Join-Path $packageDirectory '*') -DestinationPath (Join-Path $projectRoot 'dist\Turnforge-Windows.zip') -Force
    Write-Output ('Windows package: ' + $packageDirectory)
}
Write-Output ('Game: ' + (Join-Path $buildDirectory 'turnforge.exe'))
