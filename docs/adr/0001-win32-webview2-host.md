# ADR 0001: C++ Win32 + WebView2 host with loopback HTTP

**Status:** Accepted  
**Date:** 2026-11-01

## Context
The assignment requires a standalone `.exe` that opens a web UI on Windows 11 without a terminal,
admin rights, or internet access. Electron requires Node.js; Tauri requires Rust; raw Win32 is
self-contained and well-understood.

## Decision
Use a Win32 application hosting WebView2 (Microsoft's Chromium-based embedded browser) as the
UI renderer. The backend is a loopback-only C++ HTTP server (cpp-httplib) on a random port.
The UI is a static Vite build served by the server.

## Consequences
- **Positive:** No Node.js runtime, no electron redistributable, fully native, smallest distribution.
- **Positive:** WebView2 runtime is pre-installed on Windows 11; browser fallback covers edge cases.
- **Negative:** Windows-only host code (mitigated: server and core are cross-platform).
- **Negative:** SmartScreen warning on first run of unsigned exe — documented in README.
- **Negative:** WebView2 user data folder in `%LOCALAPPDATA%` (never next to exe — security practice).