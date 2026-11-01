# tools/verify_package.ps1 -- Comprehensive verification of the packaged dist/ per Requirement Z1 & Phase 8.
# Exits with code 1 on any failure.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$dist = Join-Path $root "dist"

if (-not (Test-Path $dist)) {
    Write-Host "ERROR: dist/ directory does not exist. Run .\tools\package.ps1 first." -ForegroundColor Red
    exit 1
}

$errors = @()
$warnings = @()

Write-Host "=== Verifying Package Contents (Requirement Z1) ===" -ForegroundColor Cyan

# 1. Required files and directories
$requiredFiles = @(
    "Launch.exe",
    "app\ui\index.html",
    "CMakeLists.txt",
    "CMakePresets.json",
    "third_party\VERSIONS.md",
    ".vscode\tasks.json",
    "README.md",
    "docs\REPORT.md",
    "docs\DEMO_SCRIPT.md",
    "docs\OWNERSHIP.md",
    "docs\AI_USE_LOG.md",
    "docs\SUBMISSION_CHECKLIST.md",
    "docs\BUILD_FROM_SOURCE.md",
    "docs\TRACEABILITY.md",
    "docs\KNOWN_LIMITATIONS.md",
    "docs\TEST_RESULTS.md",
    "tools\make_git_evidence.ps1"
)

foreach ($f in $requiredFiles) {
    $target = Join-Path $dist $f
    if (-not (Test-Path $target)) {
        $errors += "MISSING required file: $f"
    } else {
        Write-Host "  [OK] Found file: $f" -ForegroundColor Green
    }
}

$requiredDirs = @(
    "src\core",
    "src\server",
    "src\host",
    "src\platform",
    "src\ui",
    "data",
    "tests",
    "results",
    "docs"
)

foreach ($d in $requiredDirs) {
    $target = Join-Path $dist $d
    if (-not (Test-Path $target)) {
        $errors += "MISSING required directory: $d"
    } else {
        Write-Host "  [OK] Found directory: $d" -ForegroundColor Green
    }
}

# 2. Check for forbidden build artifacts and oversized caches
Write-Host "`n=== Verifying Absence of Build Artifacts ===" -ForegroundColor Cyan
$forbiddenExtensions = @("*.pdb", "*.ilk", "*.obj", "*.tlog", "*.lastbuildstate", "*.idb")
foreach ($ext in $forbiddenExtensions) {
    $found = Get-ChildItem $dist -Recurse -Include $ext -ErrorAction SilentlyContinue
    foreach ($f in $found) {
        $rel = $f.FullName.Substring($dist.Length)
        $errors += "FORBIDDEN build artifact found: $rel"
    }
}

# Forbidden directories
$forbiddenDirs = @("node_modules", ".vs", "build", "CMakeFiles")
foreach ($d in $forbiddenDirs) {
    $foundDirs = Get-ChildItem $dist -Recurse -Directory -Filter $d -ErrorAction SilentlyContinue
    foreach ($fd in $foundDirs) {
        $rel = $fd.FullName.Substring($dist.Length)
        $errors += "FORBIDDEN directory present: $rel"
    }
}

# 3. Check for absolute paths
Write-Host "`n=== Scanning for Absolute Paths ===" -ForegroundColor Cyan
$textExtensions = @("*.md", "*.txt", "*.json", "*.html", "*.cpp", "*.hpp", "*.h", "*.ts", "*.tsx", "*.ps1")
$scannedFiles = Get-ChildItem $dist -Recurse -Include $textExtensions -ErrorAction SilentlyContinue
$absPatterns = @('C:\', 'D:\', 'E:\', 'F:\', '/home/', '/Users/', '/root/')

$whitelistedFiles = @("KNOWN_LIMITATIONS.md", "TEST_RESULTS.md", "verify_package.ps1", "catch_amalgamated.cpp", "README.md", "SUBMISSION_CHECKLIST.md", "DEMO_SCRIPT.md")

foreach ($file in $scannedFiles) {
    if ($whitelistedFiles -contains $file.Name) {
        continue
    }
    $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    foreach ($pat in $absPatterns) {
        if ($content.Contains($pat)) {
            $rel = $file.FullName.Substring($dist.Length)
            $errors += "ABSOLUTE PATH pattern '$pat' found in: $rel"
        }
    }
}

# 4. Check for credentials and sensitive data patterns
Write-Host "`n=== Scanning for Credentials and Sensitive Data ===" -ForegroundColor Cyan
$secretRegexes = @(
    'api[_-]?key\s*[:=]\s*["''][a-zA-Z0-9_\-]{16,}["'']',
    'password\s*[:=]\s*["''][^"'']{6,}["'']',
    'secret\s*[:=]\s*["''][a-zA-Z0-9_\-]{16,}["'']',
    'bearer\s+[a-zA-Z0-9_\-\.]{25,}',
    '-----BEGIN\s+(RSA|EC|DSA|OPENSSH)?\s*PRIVATE KEY-----'
)

foreach ($file in $scannedFiles) {
    $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    foreach ($regex in $secretRegexes) {
        if ($content -match $regex) {
            $rel = $file.FullName.Substring($dist.Length)
            $errors += "POTENTIAL CREDENTIAL or SECRET pattern match in: $rel"
        }
    }
}

# 5. Summary and Exit Code
Write-Host "`n==============================================="
if ($errors.Count -gt 0) {
    Write-Host "PACKAGE VERIFICATION FAILED ($($errors.Count) errors):" -ForegroundColor Red
    foreach ($err in $errors) {
        Write-Host "  [X] $err" -ForegroundColor Red
    }
    exit 1
} else {
    $fileCount = (Get-ChildItem $dist -Recurse -File).Count
    $totalSize = (Get-ChildItem $dist -Recurse | Measure-Object -Property Length -Sum).Sum
    Write-Host "PACKAGE VERIFICATION PASSED cleanly!" -ForegroundColor Green
    Write-Host "  Total Files: $fileCount"
    Write-Host "  Unpacked Dist Size: $([math]::Round($totalSize / 1MB, 2)) MB"
    Write-Host "==============================================="
    exit 0
}
