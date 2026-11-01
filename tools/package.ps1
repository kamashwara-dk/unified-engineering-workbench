# tools/package.ps1 -- Build Release and assemble the distributable ZIP per Requirement Z1.
# Usage: .\tools\package.ps1 -CandidateId <id>
# No machine-specific paths. Must run from the repo root.
param(
    [Parameter(Mandatory=$true)][string]$CandidateId
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

$cmake = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) {
    $cmake = "$env:ProgramFiles\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
}
if (-not (Test-Path $cmake)) {
    $cmake = "cmake"  # fall back to PATH
}

Write-Host "=== Configuring ==="
& $cmake --preset vs2022
Write-Host "=== Building Release ==="
& $cmake --build --preset vs2022-release
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$dist = Join-Path $root "dist"
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory $dist | Out-Null

# 1. One-click Launch.exe in dist root (Req A1, Z1)
$launch = Join-Path $root "build\vs2022\src\host\Release\Launch.exe"
if (-not (Test-Path $launch)) { throw "Launch.exe not found at: $launch" }
Copy-Item $launch $dist

# 2. Production UI distribution in dist/app/ui
$uiSource = Join-Path $root "src\ui\dist"
if (-not (Test-Path $uiSource)) {
    Write-Host "=== Building UI ==="
    Push-Location (Join-Path $root "src\ui")
    & npm.cmd run build
    Pop-Location
}
if (-not (Test-Path $uiSource)) { throw "UI build directory not found at: $uiSource" }
$uiDest = Join-Path $dist "app\ui"
New-Item -ItemType Directory -Force $uiDest | Out-Null
Copy-Item (Join-Path $uiSource "*") $uiDest -Recurse

# 3. Source tree (excluding node_modules and transient dist)
$srcDest = Join-Path $dist "src"
New-Item -ItemType Directory -Force $srcDest | Out-Null
# Copy host, core, server, platform
foreach ($dir in @("core", "server", "host", "platform")) {
    $srcSub = Join-Path $root "src\$dir"
    if (Test-Path $srcSub) {
        Copy-Item $srcSub $srcDest -Recurse
    }
}
# Copy src/ui sources excluding node_modules and dist
$uiSrcDest = Join-Path $srcDest "ui"
New-Item -ItemType Directory -Force $uiSrcDest | Out-Null
Get-ChildItem (Join-Path $root "src\ui") -Exclude "node_modules", "dist" | ForEach-Object {
    Copy-Item $_.FullName $uiSrcDest -Recurse
}

# 4. Build configuration & editor config
Copy-Item (Join-Path $root "CMakeLists.txt") $dist
Copy-Item (Join-Path $root "CMakePresets.json") $dist
Copy-Item (Join-Path $root ".editorconfig") $dist -ErrorAction SilentlyContinue
Copy-Item (Join-Path $root ".clang-format") $dist -ErrorAction SilentlyContinue
Copy-Item (Join-Path $root ".gitignore") $dist -ErrorAction SilentlyContinue

if (Test-Path (Join-Path $root ".vscode")) {
    $vscodeDest = Join-Path $dist ".vscode"
    New-Item -ItemType Directory -Force $vscodeDest | Out-Null
    Copy-Item (Join-Path $root ".vscode\*") $vscodeDest -Recurse
}

# 5. Dependency / version info and third-party licences
if (Test-Path (Join-Path $root "third_party")) {
    $tpDest = Join-Path $dist "third_party"
    New-Item -ItemType Directory -Force $tpDest | Out-Null
    Copy-Item (Join-Path $root "third_party\*") $tpDest -Recurse
}

# 6. Data directory with provenance
if (Test-Path (Join-Path $root "data")) {
    $dataDest = Join-Path $dist "data"
    New-Item -ItemType Directory -Force $dataDest | Out-Null
    Copy-Item (Join-Path $root "data\*") $dataDest -Recurse
}

# 7. Tests
if (Test-Path (Join-Path $root "tests")) {
    $testsDest = Join-Path $dist "tests"
    New-Item -ItemType Directory -Force $testsDest | Out-Null
    Copy-Item (Join-Path $root "tests\*") $testsDest -Recurse
}

# 8. Results
if (Test-Path (Join-Path $root "results")) {
    $resultsDest = Join-Path $dist "results"
    New-Item -ItemType Directory -Force $resultsDest | Out-Null
    Copy-Item (Join-Path $root "results\*") $resultsDest -Recurse
}

# 9. Tools
if (Test-Path (Join-Path $root "tools")) {
    $toolsDest = Join-Path $dist "tools"
    New-Item -ItemType Directory -Force $toolsDest | Out-Null
    Copy-Item (Join-Path $root "tools\*") $toolsDest -Recurse
}

# 10. Documentation and README
Copy-Item (Join-Path $root "README.md") $dist
$docsDest = Join-Path $dist "docs"
New-Item -ItemType Directory -Force $docsDest | Out-Null
Copy-Item (Join-Path $root "docs\*") $docsDest -Recurse -Exclude "*.tmp"

# 11. Assemble distributable ZIP
$zipName = "${CandidateId}_Workbench.zip"
$zipPath = Join-Path $root $zipName
if (Test-Path $zipPath) { Remove-Item $zipPath }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($dist, $zipPath)
Write-Host "=== Package created: $zipName ($([math]::Round((Get-Item $zipPath).Length/1MB,1)) MB) ==="
