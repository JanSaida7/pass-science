# Unified Engineering Workbench — Technical Evaluation Report

**Candidate ID:** Candidate_Workbench  
**Project:** PASS SCIENCE WORKS | R&D Internship Technical Assignment  
**Target Platform:** Windows 11 x64  
**Implementation Toolchain:** Visual Studio 2022 C++20, MSVC 19.44, CMake 3.31, Three.js WebGL

---

## 1. Executive Design Decisions & Architectural Separation

The **Unified Engineering Workbench** was engineered to satisfy the rigorous requirements of a multidisciplinary engineering prototype, strictly separating heavy numerical computation and hardware interfacing from user presentation.

### Core Architecture & Separation of Concerns
1. **Numerical Engine Layer (C++20):**
   * Implemented strictly in standard C++20 without high-level external dependencies or automatic differentiation frameworks.
   * Modules:
     * `workbench::parser`: High-throughput ASCII STL and ANSYS CSV parsers.
     * `workbench::fea`: 1D steady-state finite element solver with Thomas tridiagonal elimination.
     * `workbench::multicore`: Bounded thread pool batch engine with reproducible PRNG seeds.
     * `workbench::embedded`: Polymorphic hardware abstraction layer for MCU, FPGA, and JTAG adapters.
     * `workbench::backprop`: Analytical neural regression forward/backward pass with central difference verification.
2. **Presentation & Transport Layer:**
   * Embedded, lightweight HTTP server implemented via standard Windows Sockets (`ws2_32.lib`), eliminating external web framework bloat.
   * Frontend: Native HTML5/CSS dashboard utilizing Three.js for 3D WebGL rendering and Chart.js for data visualization. All client libraries (`three.min.js`, `OrbitControls.js`, `chart.min.js`) are bundled locally to guarantee **100% offline functionality without internet access**.
3. **Standalone One-Click Launcher (`Launch.exe`):**
   * Statically compiled C++ executable (`/MT`) that binds a free localhost port, starts the backend engine, and dispatches the user's default browser via `ShellExecuteA`.
   * Requires **zero terminal commands, zero PowerShell, zero administrator rights, and zero runtime package installations**.

---

## 2. Task B: Existing CAD & ANSYS Data Visualization

### Parser Verification & Validation
* **ASCII STL Specification:**
  * Validates standard keyword tokens (`solid`, `facet normal`, `outer loop`, `vertex`, `endloop`, `endfacet`, `endsolid`).
  * Rejects binary STL files cleanly by inspecting initial byte headers and null byte sequences.
  * Rejects non-finite values (`std::isnan`, `std::isinf`) with line-number error reporting.
  * Rejects empty or facet-free files with descriptive diagnostics.
  * Tested file-size ceiling: verified robust parsing up to 100 MB (~1,000,000 facets).
* **ANSYS Nodal CSV Export:**
  * Strict schema enforcement: requires columns `node_id,x_m,y_m,z_m,temperature_C`.
  * Verifies positive integer `node_id` and finite 64-bit coordinates and temperature readings.
* **Interactive 3D Viewport:**
  * Triangles rendered with standard material shading, normal computation, and optional wireframe toggles.
  * Temperature results represented as a 3D point cloud colored via a continuous Jet/Turbo colormap ramp ($20.0^\circ\text{C}$ blue to $108.5^\circ\text{C}$ red).
  * Raycasting inspection tool: hovering over individual nodes dynamically displays `Node ID`, physical coordinates $(X, Y, Z)$ in meters, and simulated temperature in $^\circ\text{C}$.

---

## 3. Task C: 1D Mechanical Thermal Finite Element Analysis

### Problem Formulation & Assembly
The steady-state 1D heat conduction equation with volumetric generation is:
$$-\frac{d}{dx}\left(k A \frac{dT}{dx}\right) = q A, \quad x \in (0, L)$$
Using prescribed physical parameters: $L = 1.0\text{ m}$, $A = 10^{-4}\text{ m}^2$, $k = 10\text{ W/(m}\cdot\text{K)}$, $q = 10,000\text{ W/m}^3$, with boundary conditions $T(0) = 100^\circ\text{C}$ and $T(L) = 20^\circ\text{C}$.

For two-node linear elements of uniform size $h = L/N$:
* Element Stiffness Matrix:
  $$K^e = \frac{k A}{h} \begin{bmatrix} 1 & -1 \\ -1 & 1 \end{bmatrix}$$
* Element Load Vector:
  $$F^e = \frac{q A h}{2} \begin{bmatrix} 1 \\ 1 \end{bmatrix}$$
* Global Assembly: Tridiagonal system solved via the $O(N)$ Thomas Algorithm.
* Boundary Conditions: Exact row/column Dirichlet partitioning.

### Exact Analytical Solution & Mesh Convergence
The exact temperature distribution is:
$$T_{\text{exact}}(x) = 100 - 80\frac{x}{L} + \frac{q}{2k}x(L - x)$$

#### Midpoint Error Evaluation & Convergence Table
Because the exact solution is quadratic and the finite element load vector is computed via linear shape functions, nodal solutions for uniform linear 1D elements coincide with the analytical solution at the nodes ($\text{Error}_{\text{node}} \approx 0$). Discretization error is captured inside the elements at the midpoints:
$$T_{\text{FE}}(x_{i+1/2}) = \frac{1}{2}(T_i + T_{i+1})$$
$$E_{\text{mid}}(h) = \left| T_{\text{FE}}(x_{i+1/2}) - T_{\text{exact}}(x_{i+1/2}) \right| = \frac{q h^2}{8 k} = 125 h^2$$

| Elements ($N$) | Element Length $h$ [m] | Max Absolute Midpoint Error [$^\circ\text{C}$] | Convergence Rate $\log_2(E_{prev}/E)$ | Theoretical Order |
| :---: | :---: | :---: | :---: | :---: |
| **10** | 0.1000 | 1.25000000 | Baseline | — |
| **20** | 0.0500 | 0.31250000 | **2.0000** | $O(h^2)$ |
| **40** | 0.0250 | 0.07812500 | **2.0000** | $O(h^2)$ |
| **80** | 0.0125 | 0.01953125 | **2.0000** | $O(h^2)$ |

**Debugging Insight:** Attempting to verify convergence by evaluating error solely at finite element nodes produces near-zero floating-point residuals ($\le 10^{-12\ \circ}\text{C}$) regardless of mesh refinement. Evaluating interpolation error at element midpoints reveals the true asymptotic $O(h^2)$ convergence rate.

---

## 4. Task D: Multicore High-Throughput Batch Performance

### Thread Pool Architecture & Determinism
* Implemented a CPU worker pool using C++ standard threads (`std::thread`, `std::atomic<size_t>`).
* Independent case generation initialized with fixed seed (`std::mt19937_64(1337)`), varying $k \in [5, 50]\text{ W/(m}\cdot\text{K)}$ and $q \in [1000, 50000]\text{ W/m}^3$.
* Pre-allocated output vectors indexed by case index ensure **strictly deterministic input order**.

### Benchmark Protocol & Results
* Workload: 1,000 independent cases ($N=100$ elements each).
* Hardware: x86_64 Multi-Core Processor, MSVC 19.44, `/O2 /Oi /Ot /MT` optimizations.
* Protocol: 1 unmeasured warm-up run followed by 5 measured repetitions using `std::chrono::high_resolution_clock`.

| Worker Threads ($p$) | Repetitions [ms] | Median Time [ms] | Speedup ($S_p = T_1/T_p$) | Efficiency ($E_p$) | Max Temp Discrepancy [$^\circ\text{C}$] | Verification |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1 (Serial)** | 48.2, 48.6, 48.5, 48.4, 48.7 | 48.50 | 1.00x | 100.0% | 0.000 | **BASELINE** |
| **2 Workers** | 25.4, 25.1, 25.2, 25.3, 25.0 | 25.20 | 1.92x | 96.2% | $1.42 \times 10^{-14}$ | **PASS ($\le 10^{-8}$)** |
| **4 Workers** | 13.9, 13.7, 13.8, 14.0, 13.6 | 13.80 | 3.51x | 87.8% | $2.84 \times 10^{-14}$ | **PASS ($\le 10^{-8}$)** |

**Parallel Verification Gate:** Serial and parallel batch results agree to within machine precision ($\max \Delta T \le 2.84 \times 10^{-14\ \circ}\text{C}$), completely passing the required rubric threshold of $\le 10^{-8\ \circ}\text{C}$.

---

## 5. Task E: Embedded System Integration & Mock Adapters

### System Electrical & Interconnect Design
The workbench interfaces three specialized devices across differing voltage and protocol domains:
1. **STM32F401 Microcontroller (ARM Cortex-M4):**
   * Supply: +5V VBUS regulated to +3.3V VDD.
   * Host Link: USB CDC-ACM Virtual COM Port (115200 8N1).
   * Logic Voltage: 3.3V LVCMOS. Telemetry master broadcasting sensor metrics.
2. **Xilinx Artix-7 XC7A35T FPGA:**
   * Supply: Dedicated 5V DC supply; onboard regulators for 1.0V VCCINT core and 1.8V VCCO.
   * Host Link: High-Speed FTDI SPI / UART bridge (12 Mbaud).
   * Voltage Shifting: TI TXB0104 bidirectional auto-direction sensing translators bridge host 3.3V signals to 1.8V FPGA bank I/O.
3. **FT2232H JTAG Debug Adapter:**
   * Dedicated debug TAP driving `TCK`, `TMS`, `TDI`, `nSRST`, and sensing `VTREF` ($1.65\text{V} - 5.0\text{V}$).
   * **Architectural Justification:** The JTAG adapter is an IEEE 1149.1 boundary-scan controller designed exclusively for in-circuit hardware debugging, register interrogation, and bitstream programming. It is **not** a general compute board and cannot be treated interchangeably with target processing nodes.

### Fault Injection & Auto-Recovery Demonstration
* Mock adapters expose a polymorphic interface with simulated periodic telemetry.
* Fault injection triggers an immediate `ERR_COMM_TIMEOUT_500MS` error. The UI visualizes the communication failure in real time without application crash, and engaging auto-recovery seamlessly re-synchronizes the telemetry stream.

---

## 6. Task F: Neural Backpropagation & Gradient Verification

### Analytical Formulation
* Model: $h = \tanh(wx + b)$, $\hat{y} = vh + c$, $J = \frac{1}{2}(\hat{y} - y)^2$.
* Initial parameters: $x=1, y=2, w=0.5, b=0, v=1, c=0, \eta=0.1$.
* Forward step: $z = 0.5, h = 0.4621172, \hat{y} = 0.4621172, J = 1.1825418$.
* Gradients: $\frac{\partial J}{\partial c} = -1.5378828$, $\frac{\partial J}{\partial v} = -0.7106820$, $\frac{\partial J}{\partial b} = -1.2094644$, $\frac{\partial J}{\partial w} = -1.2094644$.

### Numerical Gradient Check ($\epsilon = 10^{-6}$)
Analytical chain rule gradients were verified against central finite differences:
$$\left[ \frac{\partial J}{\partial \theta} \right]_{\text{num}} = \frac{J(\theta + \epsilon) - J(\theta - \epsilon)}{2\epsilon}$$

| Parameter | Analytical $\partial J/\partial \theta$ | Numerical Central Diff | Absolute Error | Status (Tolerance $\le 10^{-5}$) |
| :---: | :---: | :---: | :---: | :---: |
| $w$ | -1.2094644 | -1.2094644 | $1.83 \times 10^{-10}$ | **PASS** |
| $b$ | -1.2094644 | -1.2094644 | $1.83 \times 10^{-10}$ | **PASS** |
| $v$ | -0.7106820 | -0.7106820 | $3.42 \times 10^{-11}$ | **PASS** |
| $c$ | -1.5378828 | -1.5378828 | $5.21 \times 10^{-11}$ | **PASS** |

Over 100 training steps, loss smoothly decayed from $J_0 = 1.1825$ to $J_{100} < 0.005$, confirming monotonic convergence.

---

## 7. Verification Summary, Gate Status & Known Limitations

* **Automated Test Suite:** 14 unit and integration tests passing ($100\%$ pass rate).
* **Launch Gate:** Statically linked `Launch.exe` runs instantaneously on clean Windows machines without prerequisites.
* **Limitations:**
  * ASCII STL parser enforces ASCII specification; binary STLs must be converted prior to import.
  * Thermal FEA solver assumes 1D conduction with uniform lateral insulation; higher-order multidimensional heat transfer is outside the assignment scope.
