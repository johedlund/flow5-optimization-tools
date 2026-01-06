# Specification: Flow5 Python Bridge (pyflow5)

This document defines the high-level design for a Python bridge to the Flow5 C++ backend using `pybind11`. This bridge allows the Foilizer optimizer to drive Flow5's high-performance solvers directly in memory.

## 1. Core Objectives
*   **Zero Disk I/O:** All geometry updates and analysis results passed via RAM.
*   **Yield Preservation:** Leverage Flow5's superior XFOIL iteration stability.
*   **3D Readiness:** Provide a path to full wing VLM/Panel optimization.

## 2. Targeted Class Mappings

### 2.1 Module: `geometry`
Maps Flow5's spline-based geometry system to Python.

| C++ Class | Python Proxy | Key Methods to Expose |
|:---|:---|:---|
| `Foil` | `Foil` | `setBaseNodes(nodes)`, `initGeometry()`, `rePanel(n, amp)`, `maxThickness()`, `maxCamber()` |
| `Node2d` | `Node2d` | `x`, `y` coordinates |

### 2.2 Module: `analysis`
Exposes the native C++ XFOIL and 3D solvers.

| C++ Class | Python Proxy | Key Methods to Expose |
|:---|:---|:---|
| `XFoilTask` | `XFoilTask` | `initialize(foil, polar, bKeepOpps)`, `run()`, `operatingPoints()` |
| `OpPoint` | `OperatingPoint` | `m_Alpha`, `m_Cl`, `m_Cd`, `m_Cpv` (Viscous Cp), `m_Cpi` (Inviscid Cp) |

## 3. Global State and Memory Management
Flow5 relies on a global object registry. The bridge must handle this carefully:
*   **Startup:** `globals::clearLog()` should be called on module load.
*   **Cleanup:** `globals::deleteObjects()` MUST be exposed and called by the Python user (or via a context manager `with pyflow5.Session(): ...`).
*   **Lifetime:** Python objects should hold `shared_ptr` or managed references to ensure Flow5's internal database stays consistent.

## 4. The "Bridge" Pseudo-Code Interface

```cpp
// pyflow5_bridge.cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "api.h"
#include "foil.h"
#include "xfoiltask.h"

namespace py = pybind11;

PYBIND11_MODULE(pyflow5, m) {
    m.def("delete_objects", &globals::deleteObjects);

    py::class_<Node2d>(m, "Node2d")
        .def(py::init<double, double>())
        .def_readwrite("x", &Node2d::x)
        .def_readwrite("y", &Node2d::y);

    py::class_<Foil>(m, "Foil")
        .def("setBaseNodes", &Foil::setBaseNodes)
        .def("initGeometry", &Foil::initGeometry)
        .def("maxThickness", &Foil::maxThickness)
        .def("rePanel", &Foil::rePanel);

    py::class_<OpPoint>(m, "OperatingPoint")
        .def_readonly("alpha", &OpPoint::m_Alpha)
        .def_readonly("cl", &OpPoint::m_Cl)
        .def_readonly("cd", &OpPoint::m_Cd);

    py::class_<XFoilTask>(m, "XFoilTask")
        .def(py::init<>())
        .def("initialize", &XFoilTask::initialize)
        .def("run", &XFoilTask::run)
        .def("operatingPoints", &XFoilTask::operatingPoints);
}
```

## 4. Operational Workflow in Python

With this bridge, the Foilizer optimizer loop becomes:

```python
import pyflow5
import numpy as np

# 1. Setup Flow5 Engine
engine_foil = pyflow5.make_naca_foil(2412, "RootSection")
task = pyflow5.XFoilTask()

def objective_function(control_points):
    # 2. Update geometry in C++ memory
    # (Uses Flow5's internal spline logic for smoothness)
    engine_foil.update_from_vec(control_points)
    
    # 3. Direct XFOIL execution (No subprocess!)
    task.initialize(engine_foil, Re=1e6, alpha=2.0)
    success = task.run()
    
    if not success:
        return 1e10 # High penalty for non-convergence
        
    result = task.results()[0]
    
    # 4. Multi-objective return
    return result.cd + penalty_for_cavitation(result.cp_upper)
```

## 5. Implementation Roadmap
1.  **Skeleton:** Compile a basic `pybind11` module that merely returns Flow5's version string.
2.  **2D Minimal:** Bind `Foil` and a single-point `XFoilTask`.
3.  **3D Bridge:** Bind `PlaneXfl` and `LLTTask` to solve the induced angle problem.
4.  **Full Integration:** Replace the `core/xfoil_interface.py` in Foilizer with a `core/flow5_interface.py`.

## 6. Strategic Advantage
By adopting this spec, Foilizer stops fighting with XFOIL's Fortran legacy and starts leveraging Flow5's modern, stable, and multi-threaded C++ architecture. It moves from being a "utility" to a "professional platform."
