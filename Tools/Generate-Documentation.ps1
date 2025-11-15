#Requires -Version 5.1
<#
.SYNOPSIS
    Generates API documentation for HomeKit ADK

.DESCRIPTION
    PowerShell equivalent of Tools/generate_api_doc.sh
    Generates HTML/Markdown API documentation using Doxygen

.PARAMETER OutputFormat
    Documentation format: HTML, Markdown, or Both

.PARAMETER OutputPath
    Output directory for generated documentation

.EXAMPLE
    .\Generate-Documentation.ps1

.EXAMPLE
    .\Generate-Documentation.ps1 -OutputFormat Markdown -OutputPath "docs\api"

.NOTES
    Date: November 15, 2025
    Requires: Doxygen (choco install doxygen)
#>

param(
    [Parameter()]
    [ValidateSet('HTML', 'Markdown', 'Both')]
    [string]$OutputFormat = 'HTML',

    [Parameter()]
    [string]$OutputPath = 'docs\api'
)

$ErrorActionPreference = 'Stop'

function Write-Info { Write-Host $args -ForegroundColor Cyan }
function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Warning { Write-Host $args -ForegroundColor Yellow }
function Write-Failure { Write-Host $args -ForegroundColor Red }

Write-Info "========================================="
Write-Info "HomeKit ADK Documentation Generator"
Write-Info "========================================="

# Check for Doxygen
if (-not (Get-Command doxygen -ErrorAction SilentlyContinue)) {
    Write-Failure "Doxygen not found!"
    Write-Info "Install with: choco install doxygen"
    Write-Info "Or download from: https://www.doxygen.nl/download.html"
    exit 1
}

Write-Success "✓ Found Doxygen: $(doxygen --version)"

# Create output directory
if (-not (Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
}

# Generate Doxyfile if not exists
$doxyfile = "Doxyfile.windows"
if (-not (Test-Path $doxyfile)) {
    Write-Info "Generating Doxyfile configuration..."

    $config = @"
PROJECT_NAME           = "HomeKit ADK"
PROJECT_NUMBER         = "1.0.0"
PROJECT_BRIEF          = "Apple HomeKit Accessory Protocol Implementation for Windows"
OUTPUT_DIRECTORY       = $OutputPath
INPUT                  = HAP PAL/Windows README.md
RECURSIVE              = YES
GENERATE_HTML          = $(if ($OutputFormat -in @('HTML','Both')) {'YES'} else {'NO'})
GENERATE_MARKDOWN      = $(if ($OutputFormat -in @('Markdown','Both')) {'YES'} else {'NO'})
GENERATE_LATEX         = NO
EXTRACT_ALL            = YES
EXTRACT_PRIVATE        = NO
EXTRACT_STATIC         = YES
HIDE_UNDOC_MEMBERS     = NO
HIDE_UNDOC_CLASSES     = NO
OPTIMIZE_OUTPUT_FOR_C  = YES
MARKDOWN_SUPPORT       = YES
AUTOLINK_SUPPORT       = YES
BUILTIN_STL_SUPPORT    = YES
SOURCE_BROWSER         = YES
INLINE_SOURCES         = NO
GENERATE_TREEVIEW      = YES
HTML_COLORSTYLE_HUE    = 220
HTML_COLORSTYLE_SAT    = 100
HTML_COLORSTYLE_GAMMA  = 80
SEARCHENGINE           = YES
"@

    $config | Out-File -FilePath $doxyfile -Encoding UTF8
    Write-Success "✓ Created $doxyfile"
}

# Run Doxygen
Write-Info "Generating documentation..."
& doxygen $doxyfile

if ($LASTEXITCODE -eq 0) {
    Write-Success "`n✓ Documentation generated successfully!"
    Write-Info "`nOutput location:"
    Write-Info "  $OutputPath"

    if ($OutputFormat -in @('HTML', 'Both')) {
        $indexPath = Join-Path $OutputPath "html\index.html"
        if (Test-Path $indexPath) {
            Write-Info "`nOpen in browser:"
            Write-Info "  start `"$indexPath`""
        }
    }

    if ($OutputFormat -in @('Markdown', 'Both')) {
        Write-Info "`nMarkdown files:"
        Write-Info "  $OutputPath\markdown\"
    }
} else {
    Write-Failure "✗ Documentation generation failed"
    exit 1
}
