#Requires -Version 5.1
<#
.SYNOPSIS
    Runs HomeKit ADK unit tests on Windows

.DESCRIPTION
    Executes all unit tests using CTest and generates a report

.PARAMETER Configuration
    Test configuration: Debug or Release

.PARAMETER Filter
    Test name filter pattern (regex)

.PARAMETER Parallel
    Number of parallel test jobs (default: CPU count)

.EXAMPLE
    .\Run-Tests.ps1

.EXAMPLE
    .\Run-Tests.ps1 -Configuration Debug -Filter "HAP.*Test"

.NOTES
    Date: November 15, 2025
#>

param(
    [Parameter()]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [Parameter()]
    [string]$Filter = '',

    [Parameter()]
    [int]$Parallel = $env:NUMBER_OF_PROCESSORS
)

$ErrorActionPreference = 'Stop'

function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Info { Write-Host $args -ForegroundColor Cyan }
function Write-Failure { Write-Host $args -ForegroundColor Red }

Write-Info "========================================="
Write-Info "HomeKit ADK Test Runner"
Write-Info "Configuration: $Configuration"
Write-Info "Parallel Jobs: $Parallel"
Write-Info "========================================="

# Find test preset
$preset = "windows-x64-$($Configuration.ToLower())"
$buildDir = "build\$preset"

if (-not (Test-Path $buildDir)) {
    Write-Failure "Build directory not found: $buildDir"
    Write-Info "Run: cmake --preset $preset && cmake --build --preset $preset"
    exit 1
}

# Run tests
Write-Info "`nRunning tests..."

$ctestArgs = @(
    '--test-dir', $buildDir,
    '--output-on-failure',
    '-j', $Parallel
)

if ($Filter) {
    $ctestArgs += @('-R', $Filter)
}

if ($Configuration -eq 'Debug') {
    $ctestArgs += @('-C', 'Debug')
} else {
    $ctestArgs += @('-C', 'Release')
}

& ctest @ctestArgs

$exitCode = $LASTEXITCODE

# Summary
Write-Info "`n========================================="
if ($exitCode -eq 0) {
    Write-Success "✓ All tests passed!"
    Write-Info "========================================="
    exit 0
} else {
    Write-Failure "✗ Some tests failed"
    Write-Info "========================================="
    Write-Info "Review output above for details"
    Write-Info "Rerun with -Verbose for more information"
    exit $exitCode
}
