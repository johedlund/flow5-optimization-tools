# UX Workflow: Foilizer + Flow5 Integration

This document describes how a user interacts with the system and how data (foils, wings, and projects) flows between the Python UI and the C++ engine.

## 1. The "Two Modes" of Operation

The system operates in two distinct tiers, depending on the complexity of the design task.

### Mode A: Autonomous Section Design (Foilizer-First)
*Best for: Quick 2D optimization, R&D of new profiles.*
1.  **Open:** User starts Foilizer and generates a NACA profile or imports a `.dat` file.
2.  **Edit:** User manipulates control points in the Foilizer canvas.
3.  **Analyze:** Every "debounced" update sends the coordinates across the **Bridge** to Flow5's C++ XFOIL. Results return instantly to the Foilizer plots.
4.  **Optimize:** User runs the optimizer. The loop stays in memory.
5.  **Save:** User saves a `.foilproj` (Foilizer's optimization state) or exports a `.dat` for general use.

### Mode B: Integrated Wing Optimization (Flow5-First)
*Best for: AC75 foils, wing-foiling wings, and full-system design.*
1.  **Open:** User opens a **Flow5 Project (`.fl5` or legacy `.xfl`)** containing a 3D wing.
2.  **Selection:** Foilizer queries the Flow5 internal database via the Bridge to list sections used in the 3D model.
3.  **Constraints:** User sets 3D objectives (e.g., "Maximize L/D of the total wing at 30 knots").
4.  **Optimization:**
    *   Foilizer tweaks the section nodes in memory.
    *   Flow5 runs a **3D VLM/Panel analysis** on the entire wing.
5.  **Save:** The project is saved using `globals::saveFl5Project()`.

---

## 2. Where does the data "Live"?

| Data Type | Primary Container | Why? |
|:---|:---|:---|
| **Foil Geometry** | Flow5 `Foil` Object | Leverages Flow5's professional spline and mesh handling. |
| **3D Plane/Wing** | Flow5 `.fl5` Project | Foilizer drives Flow5's native 3D data model. |
| **Opti Settings** | Foilizer `.foilproj` | Bounds, weights, and history are specific to the Foilizer optimizer. |
| **Results/Polars** | Flow5 `Polar` Objects | Flow5 has a refined format for storing/interpolating viscous data. |

---

## 3. The Practical User Experience

### Opening a Project
The user will have an "Open Flow5 Project" option in the File menu. This uses `pyflow5` to load the `.fl5` binary data into Flow5's memory space and populate the Foilizer sidebar.

### Modifying Sections
When the user moves a point in Foilizer, it doesn't just update a Python list; it executes `pFoil->update_points()` in the C++ backend. This ensures that the geometry being edited is **exactly** the geometry being analyzed.

### Saving Progress
*   **"Save Project"** in Foilizer will save the optimization parameters (the "recipe").
*   **"Export to Flow5"** will update the original `.f5p` file so the user can go back to Flow5 for final rendering, CAD export (IGES/STEP), or stability analysis.

## 4. Summary: The Best of Both Worlds
In this workflow, **Flow5 acts as the "Database and Physics Server,"** and **Foilizer acts as the "Design and Optimization Terminal."** 

The user gets the ease-of-use of a modern Python/Qt interface without sacrificing the speed and 3D capabilities of a world-class C++ aerodynamic suite.
