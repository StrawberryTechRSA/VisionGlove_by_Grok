# Build VisionGlove by Grok (MSYS2 MinGW g++)
$ErrorActionPreference = "Stop"

$Gpp = "C:\msys64\ucrt64\bin\g++.exe"
if (-not (Test-Path $Gpp)) {
    $Gpp = "g++"
}

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

New-Item -ItemType Directory -Force -Path build, logs | Out-Null

$Sources = @(
    "src\logger.cpp",
    "src\config.cpp",
    "src\security.cpp",
    "src\sensors.cpp",
    "src\vision.cpp",
    "src\haptics.cpp",
    "src\comms.cpp",
    "src\glove_system.cpp",
    "src\main.cpp"
)

$Out = "build\visionglove.exe"
$Args = @(
    "-std=c++20",
    "-O2",
    "-Wall",
    "-Wextra",
    "-I", "include",
    "-o", $Out
) + $Sources

Write-Host "Compiling with $Gpp ..."
& $Gpp @Args
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "OK -> $Out"
Write-Host "Run: .\build\visionglove.exe --test"
Write-Host "Demo: .\build\visionglove.exe --demo panic --persons 4 --seconds 5"
