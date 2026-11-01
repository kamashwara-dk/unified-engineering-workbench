# FEA Notes -- 1-D Thermal Rod

## Formulation

**Governing equation:** -d/dx(kA dT/dx) = qA

- Constant properties: k, A, q independent of x or T
- Lateral surfaces insulated (1-D heat flow only)
- Domain: x in [0, L]
- Boundary conditions: T(0) = T0 (Dirichlet), T(L) = TL (Dirichlet)

**Assignment values:** L = 1 m, A = 1e-4 m², k = 10 W/(m·K), q = 1e4 W/m³, T0 = 100 °C, TL = 20 °C

**Units:** All field names in the code carry their unit in the identifier (e.g. length_m, conductivity_W_per_mK, T_left_C).

## Element matrices

Two-node linear element of length h = L/N.

Stiffness matrix:  ke = (kA/h) [[1, -1], [-1, 1]]

Load vector:       fe = (qAh/2) [1, 1]

These are derived from the Galerkin weak form with linear shape functions N1 = 1 - xi, N2 = xi (xi in [0,1]).

## Assembly

A genuine element loop scatters ke and fe into the global tridiagonal arrays (lower a, main b, upper c, and RHS f).
No closed-form shortcut is used for the assembled system.

## Dirichlet boundary conditions

Symmetric elimination: for each constrained DOF i with prescribed value Ti,
subtract Ti * K_{j,i} from f_j for all j, then zero row i and column i and set K_{ii}=1, f_i=Ti.
This preserves symmetry and produces exact prescribed values in the nodal solution.

## Solver

Thomas algorithm (forward sweep + back substitution) on the tridiagonal system.
Near-zero pivot is detected and returned as a typed Status::singular() error.
Pivot tolerance: 1e-15 * max|diagonal entry|.

## Exact solution

T_exact(x) = T0 + (TL - T0) * x/L + (q/(2k)) * x * (L - x)

This satisfies the ODE and both boundary conditions exactly.

## Error metrics

**Nodal error:** max over nodes i of |T_FE(x_i) - T_exact(x_i)|
Linear elements are nodally exact for this problem (polynomial of degree 2 solved by degree-1 basis),
so the nodal error is near machine epsilon (~1e-12).

**Midpoint error:** max over elements e of |T_FE(x_e+h/2) - T_exact(x_e+h/2)|
The FE value at the midpoint is the average of the two nodal values (linear interpolation).
The exact solution is degree 2; the linear element misses the parabolic part, giving:

    midpoint error = (q / (8k)) * h^2 = 125 * h^2   [for the assignment values]

Closed form: q/(8k) = 1e4 / (8 * 10) = 125 °C/m²

Convergence order = log2(e_{N} / e_{2N}) → 2 as N → ∞ (second-order convergence).

## Verification table

Values below are from the C++ test run (`fem1d_tests` / `workbench_cli fem`) on the assignment problem ($L = 1\text{ m}, A = 10^{-4}\text{ m}^2, k = 10\text{ W}/(\text{m}\cdot\text{K}), q = 10^4\text{ W}/\text{m}^3, T_0 = 100\,^\circ\text{C}, T_L = 20\,^\circ\text{C}$).

| N  | h (m)  | Max midpoint error (°C) | Expected (125 h²) | Max nodal error (°C) | Observed order |
|:--:|:------:|:-----------------------:|:-----------------:|:--------------------:|:--------------:|
| 10 | 0.1000 | 1.250000e+00            | 1.250000          | 5.684342e-14         | NaN            |
| 20 | 0.0500 | 3.125000e-01            | 0.312500          | 2.557954e-13         | 2.000000       |
| 40 | 0.0250 | 7.812500e-02            | 0.078125          | 7.673862e-13         | 2.000000       |
| 80 | 0.0125 | 1.953125e-02            | 0.019531          | 1.136868e-12         | 2.000000       |

- Convergence order: $\log_2(e_N / e_{2N}) = 2.000000$ to six decimal places, confirming exact second-order $O(h^2)$ midpoint error decay.
- Nodal exactness: Nodal error is $< 1.2\times 10^{-12}\,^\circ\text{C}$ (within double-precision floating-point roundoff).

## Reaction forces

$R_0$ and $R_N$ are the boundary heat fluxes. Equilibrium check: $R_0 + R_N = -q \cdot A \cdot L = -10^4 \times 10^{-4} \times 1 = -1.0\text{ W}$.
Total outward flux equals minus total volumetric generation. Tested and verified in `test_fem1d.cpp`.

## Debugging insight

[MISSING: Candidate debugging insight on FEA assembly or solver implementation to be added by candidate]
