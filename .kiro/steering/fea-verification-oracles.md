---
inclusion: manual
---
# FEA verification oracles
Problem: L=1 m, A=1e-4 m², k=10 W/(m·K), q=1e4 W/m³, T(0)=100 °C, T(L)=20 °C.
Exact: T(x) = 100 - 80x/L + (q/(2k)) x (L - x).  Element: ke = kA/h [[1,-1],[-1,1]], fe = qAh/2 [1,1].

Expected results (reproduced 2026-11-01 with a throwaway script; the C++ must reproduce them independently):
| N | h (m) | max midpoint error (°C), FE interpolation | max nodal error |
|---|---|---|---|
| 10 | 0.1 | 1.25 | ~1e-13 |
| 20 | 0.05 | 0.3125 | ~1e-13 |
| 40 | 0.025 | 0.078125 | ~1e-12 |
| 80 | 0.0125 | 0.01953125 | ~1e-12 |
Closed form: midpoint error = (q/k) h² / 8 = 125 h². Observed convergence order = 2.
Why midpoints: linear elements are nodally exact here, so node-only checks hide the discretisation error.
Test tolerances: midpoint error within 1e-9 of 125 h²; nodal error < 1e-9.
Generalised exact (batch cases): T(x) = T0 + (TL - T0) x/L + (q/(2k)) x (L - x).
