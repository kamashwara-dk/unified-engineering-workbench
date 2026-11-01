# ADR 0007: Frontend Shell Architecture, Engineering Bench Instrument Design, and Offline Bundling

**Status:** Accepted
**Date:** 2026-11-01

## Context
Phase 4 requires a responsive, high-performance desktop frontend shell serving the Geometry, Thermal Analysis, Performance, and Hardware views. The interface must function completely offline on Windows 11 without runtime CDN requests, adhere to strict Content Security Policy (CSP) headers, and render 3D meshes (up to millions of triangles/nodes) and interactive analytical plots without UI freezes.
Furthermore, the UI must strictly observe engineering boundary rules: all validation authority, bounds, units, convergence rates, and analytical solutions reside in the C++ backend.

## Decision
1. **Frontend Stack**: Vite + React 18 + TypeScript (strict) + Tailwind CSS in `src/ui`, building static assets with relative base (`./`) to `src/ui/dist`.
2. **Design Language**: Engineered as a bench instrument rather than a generic SaaS dashboard. Dark graphite palette (`#111215`, `#18191e`, `#202228`) with instrument cyan accent (`#0284c7` / `#38bdf8`) and semantic status colors. Tabular monospace numerals for all measurement displays.
3. **Typography**: Self-hosted SIL Open Font License (OFL) pair (`IBM Plex Sans` and `IBM Plex Mono`) via npm `@fontsource` packages. Zero runtime network requests or CDN links.
4. **3D Visualization**: Three.js with OrbitControls. Consumes little-endian binary buffers directly via `Float32Array` and `Uint32Array` without per-point React allocations. Color-blind safe Inferno colormap LUT with °C legend.
5. **Plotting**: `plotly.js-cartesian-dist-min` for 2D engineering plots (Temperature vs Position and log-log Mesh Convergence).
6. **Motion**: Framer Motion used strictly for view navigation transitions, with immediate bypass when `prefers-reduced-motion` is active.
7. **Strict CSP Compatibility**: Built and verified against the loopback server's restrictive CSP header with zero CSP relaxations required.
8. **Packaging**: Packaging script `tools/package.ps1` builds and copies `src/ui/dist` into `dist/app/ui`.

## Consequences
- **Positive:** Complete offline determinism with zero external network access.
- **Positive:** Fast 60 FPS 3D rendering and raycasting on point clouds using native WebGL typed arrays.
- **Positive:** Full accessibility compliance (WCAG AA contrast, keyboard navigation with visible focus rings, ARIA roles, polite status announcer).
- **Positive:** Clean architectural separation: C++ backend owns validation and math; frontend owns presentation and interaction.
- **Negative:** Initial bundle size includes Three.js and Plotly.js (~2.1 MB uncompressed, ~690 KB gzip), acceptable for local desktop serving over loopback.

