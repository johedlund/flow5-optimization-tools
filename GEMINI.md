# Flow5 Project Context

## Project Overview

**Flow5** is a potential flow solver application designed for the preliminary design and analysis of wings, planes, hydrofoils, and sails. It is the successor to the legacy `xflr5` project.

The project is structured as a C++ application using the Qt framework, split into core libraries, a GUI application, and the XFoil solver engine.

### Key Technologies
*   **Language:** C++ (Qt 6)
*   **Build System:** qmake
*   **GUI Framework:** Qt Widgets
*   **Solver:** Custom implementation + XFoil port (`XFoil-lib`)

## Project Structure

*   `fl5-app/`: The main GUI application.
    *   `core/`: Application-specific logic (settings, file I/O).
    *   `interfaces/`: UI modules for different tasks (optimization, 2D view, 3D view).
    *   `modules/`: High-level application modules (`xplane`, `xdirect`).
    *   `globals/`: Application-wide globals and main entry point.
*   `fl5-lib/`: Core libraries used by the app and API.
    *   `api/`: Public API definition (`api.h`) and core data structures (`Foil`, `Polar`, `Plane`).
    *   `analysis3d/`: 3D analysis tasks (`LLT`, `Panel`, `VLM`).
    *   `geom/`: Geometry processing.
*   `XFoil-lib/`: C++ translation of the original XFOIL solver.
*   `API_examples/`: Headless examples demonstrating how to use `fl5-lib` directly without the GUI.
*   `meta/`: Packaging and OS-specific metadata.
*   `docs/`: Design documents and specifications.

## Development Workflow

### Build Instructions

The project uses `qmake`. Ensure Qt 6 is installed.

**Build all components:**
```bash
qmake6 flow5.pro
make
```

**Build individual components:**
```bash
# Build XFoil library
qmake6 XFoil-lib/XFoil-lib.pro && make -C XFoil-lib

# Build Core library
qmake6 fl5-lib/fl5-lib.pro && make -C fl5-lib

# Build Application
qmake6 fl5-app/fl5-app.pro && make -C fl5-app
```

### Running Tests

There is no formal unit test suite (e.g., GTest). Verification is done primarily through `API_examples` or manual GUI testing.

**Running an API Example (e.g., Foil Optimization):**
This often requires setting environment variables to locate the libraries.

```bash
export QT6_INCLUDE_DIR=/path/to/qt/include
export QT6_LIB_DIR=/path/to/qt/lib
export XFOIL_LIB_DIR=$(pwd)/XFoil-lib
export FL5_LIB_DIR=$(pwd)/fl5-lib

./API_examples/foiloptimize/run_test.sh
```

### Issue Tracking (Beads)

This project uses `beads` (local CLI tool) for issue tracking.

*   `bd ready`: List unblocked tasks.
*   `bd create "Title" --type task`: Create a new task.
*   When starting work on an item, set it to **in progress** (`bd update <id> --status in_progress` or `bd update <id> --claim`).
*   `bd close <id>`: Mark a task as complete.
*   **Verification:** For GUI-related tasks or crash fixes that cannot be verified via automated headless scripts, **pause and ask the user to verify** manually before closing the issue.
*   `bd sync`: Sync local issues with git.

### Architecture Notes

*   **Global Registry:** The system relies on a global object registry. When using the API, you **must** call `globals::deleteObjects()` before exiting to prevent memory leaks and ensure clean shutdown.
*   **Optimization:** Optimization tasks are located in `fl5-app/interfaces/optim/`.
*   **Threads:** Some solvers (like OpenBLAS used in splines) may conflict with multithreading. Scripts often set `OPENBLAS_NUM_THREADS=1`.

### Architectural Decisions

*   **Flow5-First C++ Implementation:**
    *   We are implementing the "Foilinizer" optimization workflow directly inside `flow5` (C++/Qt) rather than using a Python bridge or subprocesses.
    *   This avoids GPLv3 linking issues and "two Qt stacks" integration risks.
    *   We leverage the native `fl5-lib` and `XFoil-lib` APIs directly for maximum performance and stability.
    *   The UI logic from Foilinizer (optimization panel, constraints, live plots) is being ported to native Qt C++ widgets.
    *   **Geometry:** We utilize Flow5's `CubicSpline` class for foil representation, which guarantees $C^2$ continuity, obsoleting the need for explicit curvature continuity constraints found in point-based optimizers.

## Coding Conventions

*   **Style:** Qt-style C++.
    *   Classes: `CamelCase`
    *   Members: `m_variableName`
    *   Methods: `camelCase`
*   **Memory Management:** Be mindful of the global object registry.
*   **Logging:** Use `globals::pushToLog(msg)` for application-wide logging.

## Useful Commands

*   **Check Status:** `git status` (always check before starting).
*   **Sync Issues:** `bd sync`.
