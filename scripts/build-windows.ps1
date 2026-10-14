param(
    [string]$QtRoot = "$PSScriptRoot\..\work\Qt\6.8.3\mingw_64",
    [string]$CompilerRoot = "$PSScriptRoot\..\work\Qt\Tools\mingw1310_64",
    [string]$SDLRoot = "$PSScriptRoot\..\work\SDL2\SDL2-2.32.10\x86_64-w64-mingw32",
    [string]$CMake = "$PSScriptRoot\..\work\python-tools\cmake\data\bin\cmake.exe",
    [string]$Ninja = "$PSScriptRoot\..\work\python-tools\bin\ninja.exe"
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildDirectory = Join-Path $projectRoot 'build-windows'
foreach ($required in @("$QtRoot\bin\Qt6Core.dll", "$CompilerRoot\bin\g++.exe", "$SDLRoot\bin\SDL2.dll", $CMake, $Ninja)) {
    if (!(Test-Path -LiteralPath $required)) { throw "Missing build dependency: $required" }
}
$previousPath = $env:PATH
try {
    $env:PATH = "$CompilerRoot\bin;$QtRoot\bin;$previousPath"
    & $CMake -S $projectRoot -B $buildDirectory -G Ninja "-DCMAKE_MAKE_PROGRAM=$Ninja" "-DCMAKE_C_COMPILER=$CompilerRoot\bin\gcc.exe" "-DCMAKE_CXX_COMPILER=$CompilerRoot\bin\g++.exe" "-DCMAKE_PREFIX_PATH=$QtRoot;$SDLRoot" -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE) { throw 'CMake configuration failed' }
    & $CMake --build $buildDirectory --parallel
    if ($LASTEXITCODE) { throw 'Compilation failed' }
    & "$([System.IO.Path]::GetDirectoryName($CMake))\ctest.exe" --test-dir $buildDirectory --output-on-failure
    if ($LASTEXITCODE) { throw 'CTest failed' }
    Copy-Item -LiteralPath "$SDLRoot\bin\SDL2.dll" -Destination $buildDirectory
    & "$QtRoot\bin\windeployqt.exe" --release --no-translations --no-opengl-sw --compiler-runtime "$buildDirectory\daisywheel.exe" "$buildDirectory\windows-input-tests.exe" "$buildDirectory\windows-focus-tests.exe"
    if ($LASTEXITCODE) { throw 'Qt deployment failed' }
    Write-Output "Windows build ready: $buildDirectory\daisywheel.exe --windows-input"
} finally {
    $env:PATH = $previousPath
}
