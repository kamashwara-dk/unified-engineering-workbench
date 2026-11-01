# Backpropagation Implementation & Verification Notes (Phase 7)

## 1. Problem Formulation & Architecture

The backpropagation engine implements non-linear regression using a 1-neuron architecture with $\tanh$ activation:

* **Fixed Input:** $x = 1.0$
* **Target Value:** $y = 2.0$
* **Hidden Pre-activation:** $z = w \cdot x + b$
* **Hidden Activation:** $h = \tanh(z)$
* **Output Prediction:** $\hat{y} = v \cdot h + c$
* **Loss Function (Objective):** $J = \frac{1}{2} (\hat{y} - y)^2$
* **Initial Parameter Vector:** $\theta_0 = [w_0, b_0, v_0, c_0]^T = [0.5, 0.0, 1.0, 0.0]^T$
* **Learning Rate:** $\eta = 0.1$

---

## 2. Provisional AI Implementation Notice

> [!IMPORTANT]
> **Provisional Analytical Formulas Notice:**
> The analytical gradient formulas implemented in `src/core/src/backprop.cpp` (`analytic_gradients()`) were provisionally supplied by the AI assistant per the user's explicit instructions during Phase 7.
> 
> The candidate's own handwritten derivation is conducted independently and will be compared against this implementation afterwards.
> 
> The finite-difference gradient check documented below serves solely as an **internal consistency check** verifying that the C++ code correctly implements the specified calculus against numerical central differences. It **does NOT constitute independent verification** of the candidate's handwritten derivation.

---

## 3. Analytical Calculus & Chain Rule Formulas

The provisional formulas implemented in `workbench::analytic_gradients()` follow standard calculus chain rule expansions:

$$\frac{\partial J}{\partial \hat{y}} = (\hat{y} - y)$$

1. **Bias $c$ (Output bias):**
   $$\frac{\partial J}{\partial c} = \frac{\partial J}{\partial \hat{y}} \cdot \frac{\partial \hat{y}}{\partial c} = (\hat{y} - y) \cdot 1 = \hat{y} - y$$

2. **Weight $v$ (Hidden-to-output weight):**
   $$\frac{\partial J}{\partial v} = \frac{\partial J}{\partial \hat{y}} \cdot \frac{\partial \hat{y}}{\partial v} = (\hat{y} - y) \cdot h$$

3. **Intermediate Gradient $\frac{\partial J}{\partial h}$:**
   $$\frac{\partial J}{\partial h} = \frac{\partial J}{\partial \hat{y}} \cdot \frac{\partial \hat{y}}{\partial h} = (\hat{y} - y) \cdot v$$

4. **Weight $w$ (Input-to-hidden weight):**
   $$\frac{\partial J}{\partial w} = \frac{\partial J}{\partial h} \cdot \frac{dh}{dz} \cdot \frac{\partial z}{\partial w} = \frac{\partial J}{\partial h} \cdot (1 - \tanh^2(z)) \cdot x = (\hat{y} - y) \cdot v \cdot (1 - h^2) \cdot x$$

5. **Bias $b$ (Hidden bias):**
   $$\frac{\partial J}{\partial b} = \frac{\partial J}{\partial h} \cdot \frac{dh}{dz} \cdot \frac{\partial z}{\partial b} = \frac{\partial J}{\partial h} \cdot (1 - \tanh^2(z)) \cdot 1 = (\hat{y} - y) \cdot v \cdot (1 - h^2)$$

All analytical formulas are strictly isolated in a single function `analytic_gradients()` in [backprop.cpp](file:///Workbench/src/core/src/backprop.cpp#L40-L75) so they can be modified or replaced with candidate expressions without modifying any other engine components.

---

## 4. Finite-Difference Gradient Check Verification

To verify implementation accuracy and prevent silent gradient calculation bugs, central differences are evaluated for each parameter with step size $\epsilon = 10^{-6}$:

$$\left( \frac{\partial J}{\partial \theta_i} \right)_{\text{num}} = \frac{J(\theta_i + \epsilon) - J(\theta_i - \epsilon)}{2\epsilon}$$

The code requires absolute error tolerance:
$$\left| \left(\frac{\partial J}{\partial \theta_i}\right)_{\text{analytic}} - \left(\frac{\partial J}{\partial \theta_i}\right)_{\text{num}} \right| \le 10^{-5}$$

### Actual Numbers Printed by C++ Engine at $\theta_0$:

| Parameter $\theta$ | Analytic $\partial J/\partial \theta$ | Numerical (Central Diff) | Absolute Error | Tolerance | Result |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **$w$** | `-1.2094644752` | `-1.2094644751` | `1.13e-10` | $\le 10^{-5}$ | **PASS** |
| **$b$** | `-1.2094644752` | `-1.2094644751` | `1.13e-10` | $\le 10^{-5}$ | **PASS** |
| **$v$** | `-0.7106820475` | `-0.7106820474` | `5.25e-11` | $\le 10^{-5}$ | **PASS** |
| **$c$** | `-1.5378828427` | `-1.5378828426` | `1.12e-10` | $\le 10^{-5}$ | **PASS** |

All four parameters match numerical finite differences to $\approx 10^{-10}$, well within the required $10^{-5}$ threshold.

---

## 5. Simultaneous Update Mechanics & Trajectory

### Simultaneous Update Rule
Gradient descent updates must evaluate gradients for all parameters simultaneously at the original parameter values $\theta^{(k)}$ before applying parameter shifts:

$$\theta^{(k+1)} = \theta^{(k)} - \eta \nabla J(\theta^{(k)})$$

Unit test `Simultaneous update evaluates all gradients from original parameters` proves that sequential updating produces a strictly different result than simultaneous updating.

### Key Trajectory Steps

* **Step 0 (Initial Point):**
  * $w_0 = 0.5$, $b_0 = 0.0$, $v_0 = 1.0$, $c_0 = 0.0$
  * $z_0 = 0.5$, $h_0 = \tanh(0.5) \approx 0.462117157$
  * $\hat{y}_0 = 1.0 \times 0.462117157 + 0.0 = 0.462117157$
  * **$J_0 = \frac{1}{2}(0.462117157 - 2.0)^2 = 1.1825418189970167$**

* **Step 1 (First Simultaneous Update / Handwritten Comparison Step):**
  * $w_1 = 0.5 - 0.1 \times (-1.2094644752) = 0.6209464475240061$
  * $b_1 = 0.0 - 0.1 \times (-1.2094644752) = 0.1209464475240061$
  * $v_1 = 1.0 - 0.1 \times (-0.7106820475) = 1.0710682047485947$
  * $c_1 = 0.0 - 0.1 \times (-1.5378828427) = 0.1537882842739990$
  * $\hat{y}_1 = 1.0710682 \times \tanh(0.6209464 \times 1 + 0.1209464) + 0.1537883 \approx 0.8288544$
  * **$J_1 = 0.6857737650448423$**

* **Subsequent Steps (2 to 101):**
  * 100 further gradient descent iterations are recorded in `results/backprop.csv`.
  * Unit test `Loss decreases monotonically during training` confirms that $J_{k+1} \le J_k$ holds across all 101 steps.
  * **$J_{101} = 1.0804984873618949 \times 10^{-20}$**

---

## 6. My Handwritten Derivation

<!-- TODO (candidate): Insert candidate's handwritten derivation steps, intermediate values, and numerical comparisons here. Do not write or guess these values. -->

---

## 7. My Explanation of the Loss Curve

<!-- TODO (candidate): Write candidate's explanation of the loss curve trajectory, gradient directions, convergence rate, and parameter dynamics here. Do not write or guess this explanation. -->

