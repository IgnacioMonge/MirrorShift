# Canonical PC client dev loop: configure + build + ctest through the `msvc`
# presets (incremental, build tree outside Dropbox, no vcvars64, Qt test PATH
# handled by CMake). Structural choices live in CMakePresets.json.
#
# Every native tool inherits the shell's standard handles so normal CMake,
# MSBuild, compiler, and CTest progress remains visible to the caller.
#
# Packaging/deploy for release stays in build-msvc.ps1 (`make client`).
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$ClientDir = $PSScriptRoot
. (Join-Path $ClientDir "build-lib.ps1")

$cmake = (Get-Command cmake -ErrorAction Stop).Source
$ctest = (Get-Command ctest -ErrorAction Stop).Source

Invoke-ProcessChecked -FilePath $cmake -Arguments @("--preset", "msvc") `
    -Name "cmake configure" -WorkingDirectory $ClientDir

$buildArguments = @("--build", "--preset", "msvc-release", "--parallel")
if ($env:CMAKE_BUILD_PARALLEL_LEVEL) {
    $buildArguments += $env:CMAKE_BUILD_PARALLEL_LEVEL
}
Invoke-ProcessChecked -FilePath $cmake -Arguments $buildArguments `
    -Name "cmake build" -WorkingDirectory $ClientDir

Invoke-ProcessChecked -FilePath $ctest -Arguments @("--preset", "msvc-release") `
    -Name "ctest" -WorkingDirectory $ClientDir

Write-Host ""
Write-Host "PC client build+tests OK"
