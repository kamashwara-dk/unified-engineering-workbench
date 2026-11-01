# ADR 0002: Static MSVC runtime + static WebView2 loader; browser fallback

**Status:** Accepted  
**Date:** 2026-11-01

## Context
A clean Windows 11 machine may not have the Visual C++ Redistributable installed. The ZIP must
run on first extract with no installation.

## Decision
- Link the MSVC runtime statically (`/MT` for Release, `/MTd` for Debug) via `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"`.
- Link `WebView2LoaderStatic.lib` (the static loader from the NuGet package). This removes any
  dependency on `WebView2Loader.dll` being in PATH.
- If `GetAvailableCoreWebView2BrowserVersionString` fails at startup, show a clear message box,
  open the default browser at the server URL, and keep a minimal window so the user can quit.

## Consequences
- **Positive:** Zero dependencies on the target machine beyond the OS itself.
- **Positive:** Single `.exe` file without DLL concerns for MSVC runtime.
- **Negative:** Slightly larger exe (~80 KB for CRT).
- **Negative:** Browser fallback has no resize/DPI handling — acceptable for fallback path.