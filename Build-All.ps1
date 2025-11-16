#!/usr/bin/env powershell
<#
.SYNOPSIS
    Build HomeKitADK for all architectures and configurations

.DESCRIPTION
    This script builds the entire HomeKitADK solution for all supported
    platforms (x64, ARM64) and configurations (Debug, Debug-BLE, Release, Release-BLE).

.PARAMETER Configuration
    Specific configuration to build (Debug, Debug-BLE, Release, Release-BLE, or All)
    Default: All

.PARAMETER Platform
    Specific platform to build (x64, ARM64, or All)
    Default: All

.PARAMETER Clean
    Perform a clean rebuild instead of incremental build

.PARAMETER Parallel
    Maximum number of parallel builds (default: number of CPU cores)

.EXAMPLE
    .\Build-All.ps1
    Build all configurations and platforms

.EXAMPLE
    .\Build-All.ps1 -Configuration Debug -Platform x64
    Build only Debug|x64

.EXAMPLE
    .\Build-All.ps1 -Clean
    Clean rebuild all configurations
#>

param(
    [ValidateSet("Debug", "Debug-BLE", "Release", "Release-BLE", "All")]
    [string]$Configuration = "All",

    [ValidateSet("x64", "ARM64", "All")]
    [string]$Platform = "All",

    [switch]$Clean,

    [int]$Parallel = $env:NUMBER_OF_PROCESSORS
)

# Solution file
$SolutionFile = "HomeKitADK.sln"

# Check if solution exists
if (-not (Test-Path $SolutionFile)) {
    Write-Error "Solution file not found: $SolutionFile"
    exit 1
}

# Find MSBuild
$msbuildPaths = @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe"
)

$msbuild = $null
foreach ($path in $msbuildPaths) {
    if (Test-Path $path) {
        $msbuild = $path
        break
    }
}

if (-not $msbuild) {
    # Try to find via vswhere
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($vsPath) {
            $msbuild = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
        }
    }
}

if (-not $msbuild -or -not (Test-Path $msbuild)) {
    Write-Error "MSBuild not found. Please install Visual Studio 2019 or 2022."
    exit 1
}

Write-Host "Using MSBuild: $msbuild" -ForegroundColor Cyan

# Define all configurations and platforms
$allConfigurations = @("Debug", "Debug-BLE", "Release", "Release-BLE")
$allPlatforms = @("x64", "ARM64")

# Filter based on parameters
$configurationsToBuild = if ($Configuration -eq "All") { $allConfigurations } else { @($Configuration) }
$platformsToBuild = if ($Platform -eq "All") { $allPlatforms } else { @($Platform) }

# Build target
$target = if ($Clean) { "Rebuild" } else { "Build" }

Write-Host "`n=== Build Configuration ===" -ForegroundColor Cyan
Write-Host "Solution: $SolutionFile"
Write-Host "Target: $target"
Write-Host "Configurations: $($configurationsToBuild -join ', ')"
Write-Host "Platforms: $($platformsToBuild -join ', ')"
Write-Host "Max Parallel: $Parallel"
Write-Host "================================`n" -ForegroundColor Cyan

$results = @()
$totalBuilds = $configurationsToBuild.Count * $platformsToBuild.Count
$currentBuild = 0

foreach ($config in $configurationsToBuild) {
    foreach ($plat in $platformsToBuild) {
        $currentBuild++
        $buildName = "${config}|${plat}"

        Write-Host "[$currentBuild/$totalBuilds] Building $buildName..." -ForegroundColor Yellow

        $startTime = Get-Date

        $arguments = @(
            $SolutionFile,
            "/t:$target",
            "/p:Configuration=$config",
            "/p:Platform=$plat",
            "/m:$Parallel",
            "/v:minimal",
            "/nologo"
        )

        $process = Start-Process -FilePath $msbuild -ArgumentList $arguments -NoNewWindow -Wait -PassThru

        $endTime = Get-Date
        $duration = $endTime - $startTime

        $result = [PSCustomObject]@{
            Configuration = $buildName
            Success = ($process.ExitCode -eq 0)
            ExitCode = $process.ExitCode
            Duration = $duration.TotalSeconds
        }

        $results += $result

        if ($result.Success) {
            Write-Host "  SUCCESS ($([math]::Round($duration.TotalSeconds, 1))s)" -ForegroundColor Green
        } else {
            Write-Host "  FAILED (Exit code: $($process.ExitCode))" -ForegroundColor Red
        }

        Write-Host ""
    }
}

# Summary
Write-Host "`n=== Build Summary ===" -ForegroundColor Cyan
$succeeded = ($results | Where-Object { $_.Success }).Count
$failed = ($results | Where-Object { -not $_.Success }).Count
$totalTime = ($results | Measure-Object -Property Duration -Sum).Sum

Write-Host "Total builds: $totalBuilds"
Write-Host "Succeeded: $succeeded" -ForegroundColor Green
Write-Host "Failed: $failed" -ForegroundColor $(if ($failed -eq 0) { 'Green' } else { 'Red' })
Write-Host "Total time: $([math]::Round($totalTime, 1))s"
Write-Host ""

# Detailed results
$results | Format-Table -Property Configuration, Success, @{Name="Duration(s)"; Expression={[math]::Round($_.Duration, 1)}}, ExitCode -AutoSize

if ($failed -gt 0) {
    Write-Host "`nFailed builds:" -ForegroundColor Red
    $results | Where-Object { -not $_.Success } | ForEach-Object {
        Write-Host "  $($_.Configuration) (Exit code: $($_.ExitCode))" -ForegroundColor Red
    }
    exit 1
}

Write-Host "`nAll builds completed successfully!" -ForegroundColor Green
exit 0
