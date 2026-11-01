# Manual tests — Phase 1 (Windows 11, Windows Sandbox)

Run these tests yourself. Record results in the Evidence column. Update IMPLEMENTATION_PLAN.md.

## Prerequisites
- Windows 11 x64 machine (your development machine)
- Windows Sandbox (enable: Control Panel → Programs → Turn Windows Features on/off → Windows Sandbox)
- The dist/ directory must exist (run `tools/package.ps1 -CandidateId TEST` first)

---

## MT-01: Normal launch on development machine

**Steps:**
1. Run `.\tools\package.ps1 -CandidateId TEST` from repo root.
2. Open `dist/` in File Explorer.
3. Double-click `Launch.exe`.
4. If SmartScreen appears: click "More info" → "Run anyway".

**Expected:**
- A window titled "Unified Engineering Workbench" appears (no console).
- The placeholder page shows `status: ok`, a semantic version, and a non-zero `uptime_ms`.
- `launch_to_ready_ms` shows a non-null integer (set after first page load).
- No terminal window is visible.

**Record:**
- Launch succeeded? ___
- launch_to_ready_ms value: ___ ms
- SmartScreen shown? ___
- Any errors: ___

---

## MT-02: Repeat launch x4 (no state left between runs)

**Steps:**
1. Close the window from MT-01.
2. Double-click `Launch.exe` again. Note the launch_to_ready_ms.
3. Repeat for a total of 4 launches.

**Expected:** All 4 launches succeed. No residual files in dist/.

**Record (all 4):**
| Run | launch_to_ready_ms | Any errors |
|---|---|---|
| 1 | | |
| 2 | | |
| 3 | | |
| 4 | | |

---

## MT-03: Windows Sandbox clean launch

**Steps:**
1. Enable Windows Sandbox if not already enabled.
2. Copy the entire `dist/` folder to a new folder on the Desktop (to simulate "extracted ZIP").
3. Open Windows Sandbox.
4. Copy the folder into the Sandbox Desktop (drag or use clipboard).
5. Double-click `Launch.exe` from inside the Sandbox.

**Expected:**
- Launches without error, terminal, or installation prompt.
- Placeholder page loads and shows `status: ok`.
- No internet required.

**Record:**
- Sandbox launch succeeded? ___
- launch_to_ready_ms: ___ ms
- Any errors: ___

Repeat for 4 clean Sandbox launches total.

| Sandbox run | launch_to_ready_ms | Any errors |
|---|---|---|
| 1 | | |
| 2 | | |
| 3 | | |
| 4 | | |

---

## MT-04: Missing WebView2 runtime fallback (optional — hard to test)

If you have a Windows 11 machine or VM without the WebView2 runtime installed (unusual):

**Steps:**
1. Launch `dist/Launch.exe`.

**Expected:**
- A message box appears explaining the runtime is missing.
- It states the URL to download the runtime.
- The default browser opens to `http://127.0.0.1:<port>/?t=<token>`.
- A minimal window stays open with "close to quit" instructions.
- Closing the window stops the server cleanly.

**Record:**
- Tested? ___
- Fallback correct? ___

---

## MT-05: Editor task (Antigravity / VS Code)

**Steps:**
1. Open the repo folder in Antigravity IDE (or VS Code).
2. Open Command Palette → Tasks: Run Task → "Run Workbench".

**Expected:**
- Package task runs (CMake configure + build + dist assembly).
- `Launch.exe` starts automatically on completion.

**Record:**
- Editor used: ___
- Task appeared in Command Palette? ___
- Package step succeeded? ___
- Launch.exe started? ___
- Any errors: ___

---

## MT-06: Verify no absolute paths in dist/

```powershell
.\tools\verify_package.ps1
```

**Expected:** "VERIFICATION PASSED" with no absolute path warnings.

**Record:**
- PASSED / FAILED: ___
- Any warnings: ___