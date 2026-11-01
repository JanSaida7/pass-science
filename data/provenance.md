# Data Provenance and Dataset Documentation

This directory contains the sample dataset for CAD geometry visualization and ANSYS thermal results visualization, in compliance with **Task B** requirements of the PASS Science Works assignment.

---

## 1. ASCII STL Geometry: `bracket.stl`
* **Format:** Standard ASCII stereolithography (`.stl`).
* **Origin / Provenance:** Self-authored parameterized mechanical mounting bracket with flange and vertical support arms. Openly licensed under MIT License / Creative Commons Zero (CC0).
* **Physical Units:**
  * Coordinates $(X, Y, Z)$: Meters ($\text{m}$).
  * Dimensions: Length $0.10\text{ m}$ ($100\text{ mm}$), Width $0.12\text{ m}$ ($120\text{ mm}$), Height $0.05\text{ m}$ ($50\text{ mm}$).
* **Geometry Characteristics:**
  * Facet count: 20 planar triangular facets.
  * Explicit unit outward normals supplied per facet loop.
  * Verified manifold and free of non-finite or degenerate coordinates.

---

## 2. Nodal Temperature Results: `synthetic_ansys_results.csv`
* **Format:** Comma-Separated Values (`.csv`).
* **Provenance Classification:** **SYNTHETIC DATA** (Explicitly labeled as synthetic in compliance with Task B rubric instructions; not an actual export from ANSYS commercial software).
* **Schema Definition:**
  | Column Name | Data Type | Physical Dimension | Description |
  | :--- | :--- | :--- | :--- |
  | `node_id` | Integer | Dimensionless | Unique positive node identifier ($\ge 1$) |
  | `x_m` | Floating Point | Meters ($\text{m}$) | Nodal spatial X-coordinate |
  | `y_m` | Floating Point | Meters ($\text{m}$) | Nodal spatial Y-coordinate |
  | `z_m` | Floating Point | Meters ($\text{m}$) | Nodal spatial Z-coordinate |
  | `temperature_C` | Floating Point | Degrees Celsius ($^\circ\text{C}$) | Simulated steady-state temperature field |

* **Thermal Field Generation Model:**
  * Node count: 264 grid points across a $11 \times 6 \times 4$ spatial domain.
  * Domain: $x \in [0.0, 0.1]\text{ m}$, $y \in [0.0, 0.05]\text{ m}$, $z \in [0.0, 0.05]\text{ m}$.
  * Governing Profile:
    $$T(x, y, z) = 100.0 - 75.0\left(\frac{x}{0.1}\right) + 20.0 \sin\left(\pi \frac{x}{0.1}\right)\left(1 - \frac{y}{0.05}\right) - 5.0\left(\frac{z}{0.05}\right)$$
  * Range: $T_{\min} \approx 20.00^\circ\text{C}$, $T_{\max} \approx 108.54^\circ\text{C}$.

