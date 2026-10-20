param(
    [string]$BuildDirectory = 'build-ci',
    [string]$ToolchainBin = 'C:\msys64\ucrt64\bin',
    [string]$Version = '0.3.0'
)
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw 'Version must be MAJOR.MINOR.PATCH' }
$project = Split-Path $PSScriptRoot -Parent
$staging = Join-Path $project 'stage-windows'
$output = Join-Path $project 'dist'
if (Test-Path $staging) { throw 'stage-windows already exists; use a clean packaging workspace' }
New-Item -ItemType Directory -Force $output | Out-Null
$env:PATH = "$ToolchainBin;$env:PATH"
cmake --install (Join-Path $project $BuildDirectory) --prefix $staging
if ($LASTEXITCODE) { throw 'Install staging failed' }
& "$ToolchainBin\windeployqt.exe" --release --compiler-runtime --no-translations "$staging\bin\daisywheel.exe"
if ($LASTEXITCODE) { throw 'Qt deployment failed' }
Copy-Item "$ToolchainBin\SDL2.dll" "$staging\bin"
cmake "-DSTAGING=$staging" "-DTOOLCHAIN_BIN=$ToolchainBin" -P "$project\packaging\windows-runtime.cmake"
if ($LASTEXITCODE) { throw 'Runtime dependency collection failed' }
# Copy dependency notices available in the MSYS2 toolchain.
Copy-Item (Join-Path (Split-Path $ToolchainBin -Parent) 'share\licenses') "$staging\third-party-licenses" -Recurse
@'
@echo off
start "" "%~dp0bin\daisywheel.exe" --windows-input
'@ | Set-Content "$staging\Start Daisywheel.cmd" -Encoding ascii
& "$staging\bin\daisywheel.exe" --windows-input --smoke-test
if ($LASTEXITCODE) { throw 'Packaged Windows startup failed' }
Compress-Archive -Path "$staging\*" -DestinationPath "$output\daisywheel-reamade-$Version-windows-x64-portable.zip"
$iscc = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
& $iscc "/DAppVersion=$Version" "/DSourceDir=$staging" "/DOutputDir=$output" "$project\packaging\windows.iss"
if ($LASTEXITCODE) { throw 'Windows installer build failed' }
