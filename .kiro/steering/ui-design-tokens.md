---
inclusion: fileMatch
fileMatchPattern: "src/ui/**"
---
# UI design standard
Direction: engineering bench instrument, not a generic dashboard. Graphite neutrals, one accent colour,
sans + mono pair (self-hosted, OFL), tabular mono numerals for all measurements.
Temperature colormap: perceptually uniform, colour-blind-safe (inferno-style), legend labelled in °C.
Rules: tokens defined once (CSS variables), WCAG AA contrast, visible focus rings, keyboard reachable controls,
status bar with `aria-live="polite"`, respect `prefers-reduced-motion`, errors say what failed, where, and what to do.
Responsive down to ~1024 px width; plots and 3D canvas resize with the window. Label simulated data on screen.
Do not spend more than ~20% of total hours on visual polish: the rubric scores usability and clarity.
