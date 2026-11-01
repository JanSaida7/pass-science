# Unified Engineering Workbench

**PASS SCIENCE WORKS | R&D Internship Technical Assignment 2026–2027**  
**Target Platform:** Windows 11 x64  
**Author / Candidate ID:** Candidate_Workbench  

---

## 1. Prerequisites & Clean-Machine Execution

* **Zero-Install Standalone Execution (Default):**
  * **No external runtime installations required:** No Python, no Node.js, no npm, no PowerShell execution, and no administrator rights.
  * The application includes a precompiled, statically linked (`/MT`) Win32 binary: `Launch.exe`.
  * **Double-clicking `Launch.exe` immediately opens the working dashboard in your default web browser.**

* **Source Build Prerequisites (Optional Rebuild from Source):**
  * Windows 11 x64
  * Microsoft Visual Studio 2022 (MSVC v143 or Build Tools) with C++20 support
  * CMake 3.20 or newer

---

## 2. One-Click Execution Steps

### Option A: Direct Double-Click (Recommended)
1. Extract `CandidateID_Workbench.zip` to any local directory.
2. Double-click `Launch.exe` located at the root of the extracted folder.
3. The C++20 backend initializes in $\sim 100\text{ ms}$, automatically binds a free local port (default `8080`), and launches your system's default web browser to:
   ```
   http://localhost:8080
   ```
4. The dashboard is fully functional, offline-capable, and ready to use immediately.

### Option B: VS Code / Antigravity One-Click Run
1. Open this workspace folder in VS Code or Antigravity.
2. Press **F5** or navigate to the **Run and Debug** side panel and select:
   * **`Launch Workbench (Packaged Application)`**
3. The editor launches the packaged application directly in one action.

---

## 3. High-Level Architecture & Separation of Concerns

```
Unified Engineering Workbench
├── Presentation Layer (Frontend):
│   └── HTML5, Tailwind-style CSS, Three.js WebGL (3D CAD/Point Cloud), Chart.js
├── Transport Layer:
│   └── High-speed Win32 C++ Sockets (Winsock ws2_32) serving REST endpoints & static files
├── C++20 Computation Engine:
│   ├── workbench::parser     -> ASCII STL & ANSYS Nodal CSV Parsers
│   ├── workbench::fea        -> 1D Steady-State Thermal Finite Element Solver (Thomas Alg.)
│   ├── workbench::multicore  -> Bounded Thread Pool & Reproducible Benchmark Harness
│   ├── workbench::embedded   -> MCU, FPGA, and JTAG Mock Hardware Adapters
│   └── workbench::backprop   -> Analytical Gradient Backpropagation & Finite-Diff Checker
└── Standalone Launcher:
    └── Launch.exe (Self-contained Win32 C++ executable)
```

---

## 4. Supported Formats & Provenance

1. **ASCII STL Geometry (`.stl`):**
   * Supports standard ASCII stereolithography files containing planar triangular facets.
   * Supplied sample: `data/bracket.stl` (Openly licensed mechanical component, CC0).
2. **ANSYS Nodal Temperature CSV (`.csv`):**
   * Schema: `node_id,x_m,y_m,z_m,temperature_C`.
   * Supplied sample: `data/synthetic_ansys_results.csv` (Synthetic 3D conduction field, 264 nodes).
   * Documentation: See `data/provenance.md` for units, provenance, and coordinate conventions.

---

## 5. Build & Test Instructions from Source

### Running the Test Suite
To execute the automated verification test suite (14/14 tests covering FEA convergence, gradient checks, multicore determinism, and parser edge cases):
```powershell
.\bin\run_tests.exe
```

### Compiling from Source via CMake
To recompile the entire solution from source on Windows 11:
```cmd
cmd.exe /c "build.bat"
```
Or manually using CMake:
```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

---

## 6. Known Limitations

1. **Binary STL Handling:** In strict adherence to Task B specifications, binary STL files are rejected with a clear actionable error prompt instructing the user to supply ASCII STL geometry.
2. **Thermal FEA Dimensionality:** The finite element solver is formulated specifically for steady-state 1D conduction with uniform volumetric generation and insulated lateral surfaces. Higher-order 2D/3D thermal FEA is outside the assignment scope.
3. **Embedded Hardware Mocks:** Physical wiring, real silicon programming, and FPGA bitstream synthesis are simulated via C++ mock adapters exhibiting realistic physics, packet telemetry, and recoverable timeout failures.

