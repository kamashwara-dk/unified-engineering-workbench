# tools/make_git_evidence.ps1 -- Export version-control evidence per Requirement Z4.
# Usage: Run from repository root: .\tools\make_git_evidence.ps1
# Creates docs/repo.bundle and docs/COMMIT_LOG.md with personal data sanitized.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

Write-Host "=== Generating Version-Control Evidence (Requirement Z4) ===" -ForegroundColor Cyan

# Check if git is installed
$gitCmd = Get-Command git -ErrorAction SilentlyContinue
if (-not $gitCmd) {
    Write-Host "ERROR: git executable not found on PATH." -ForegroundColor Red
    exit 1
}

# Check if repository is initialized
$isRepo = (git rev-parse --is-inside-work-tree 2>$null)
if ($LASTEXITCODE -ne 0 -or $isRepo -ne 'true') {
    Write-Host "WARNING: Not currently inside a Git repository." -ForegroundColor Yellow
    Write-Host "If you have not yet initialized Git, run the following first:"
    Write-Host "  git init"
    Write-Host "  git add ."
    Write-Host "  git commit -m 'Initial submission commit'"
    Write-Host "Then rerun this script: .\tools\make_git_evidence.ps1"
    exit 1
}

$docsDir = Join-Path $root "docs"
if (-not (Test-Path $docsDir)) {
    New-Item -ItemType Directory -Force $docsDir | Out-Null
}

# 1. Create readable commit log (Requirement Z4)
$commitLogPath = Join-Path $docsDir "COMMIT_LOG.md"
Write-Host "Exporting formatted commit log to: docs/COMMIT_LOG.md"

$rawLog = git log --stat --date=iso
# Sanitize personal email or machine usernames if needed
$sanitizedLog = $rawLog -replace '<[^>]+@[^>]+>', '<candidate@submission.local>'

$header = @"
# Git Commit Log

> Generated automatically by \`tools/make_git_evidence.ps1\` per Requirement Z4.
> Formatted as \`git log --stat --date=iso\`.

\`\`\`text
"@

$footer = @"
\`\`\`
"@

Set-Content -Path $commitLogPath -Value "$header`n$sanitizedLog`n$footer" -Encoding utf8
Write-Host "  [OK] Saved docs/COMMIT_LOG.md" -ForegroundColor Green

# 2. Create Git Bundle (Requirement Z4)
$bundlePath = Join-Path $docsDir "repo.bundle"
Write-Host "Creating portable git bundle at: docs/repo.bundle"
if (Test-Path $bundlePath) { Remove-Item $bundlePath -Force }

git bundle create $bundlePath --all
if ($LASTEXITCODE -ne 0) {
    # If branches are not fully tracked, bundle HEAD
    git bundle create $bundlePath HEAD
}

if (Test-Path $bundlePath) {
    $bundleSize = [math]::Round((Get-Item $bundlePath).Length / 1KB, 1)
    Write-Host "  [OK] Saved docs/repo.bundle ($bundleSize KB)" -ForegroundColor Green
} else {
    Write-Host "ERROR: Failed to create git bundle." -ForegroundColor Red
    exit 1
}

Write-Host "`nVersion-control evidence generation complete!" -ForegroundColor Cyan
Write-Host "Artifacts created:"
Write-Host "  - docs/COMMIT_LOG.md"
Write-Host "  - docs/repo.bundle"

