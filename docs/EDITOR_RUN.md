# Editor run guide

## Prerequisites
- Windows 11 x64
- Visual Studio Build Tools 2022 (already installed; includes MSVC 19.44 and CMake 3.31.6)
- PowerShell 5.1+ (inbox with Windows 11)

## One-click run in VS Code or Antigravity IDE

1. Open the workspace root folder in the editor.
2. Open the Command Palette (`Ctrl+Shift+P`) and run **Tasks: Run Task**.
3. Choose **Run Workbench**.
   - This first runs **Package (Release)**, which configures and builds with the `vs2022` preset and assembles `dist/`.
   - Then it launches `dist/Launch.exe` directly.

The editor is **not** needed at run time. Once launched, `Launch.exe` is fully standalone.

## Manual build and run (without editor task)

```powershell
# From the repo root:
.\tools\package.ps1 -CandidateId CANDIDATE
.\dist\Launch.exe
```

## Antigravity IDE compatibility

**NOT VERIFIED.** The `.vscode/tasks.json` format is documented as supported by Antigravity,
but the tasks have not been executed inside Antigravity on this machine. The candidate must
verify and note the result in `docs/MANUAL_TESTS.md`.

## Known first-run behaviour

- **SmartScreen warning:** Windows may show "Windows protected your PC" on the first launch
  of an unsigned `.exe`. Click "More info" → "Run anyway". This is expected for unsigned binaries.
  For the assessment, the assessor should be informed in the README.
- **WebView2 runtime absent:** If the WebView2 runtime is not installed (unusual on Windows 11),
  a message box explains the situation and opens the default browser. Keep the small window
  open while using the browser; close it to stop the server.