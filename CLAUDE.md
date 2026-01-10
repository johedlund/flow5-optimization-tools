# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Git Identity

**Run this at session start** to track contributions:
```bash
export GIT_AUTHOR_NAME="Claude Agent" GIT_AUTHOR_EMAIL="claude@agent.flow5" GIT_COMMITTER_NAME="Claude Agent" GIT_COMMITTER_EMAIL="claude@agent.flow5"
```

## Project Overview

Flow5 is a potential flow solver (aerodynamic analysis) with built-in pre/post-processing. It's version 7 of the legacy xflr5 project, used for preliminary design of wings, planes, hydrofoils, and sails.

### Key Features

- **2D Foil Analysis**: XFoil-based airfoil analysis (Cl, Cd, Cm, pressure distributions)
- **3D Panel Methods**: Lifting line, VLM, and 3D panel analysis for wings/planes
- **Foil Optimization**: PSO-based airfoil shape optimization with multiple objectives
- **CAD Integration**: OpenCASCADE-based geometry import/export (STEP, STL)
- **Sail Analysis**: Specialized tools for sail design and analysis

### Foil Optimization Capabilities

The optimization system (`interfaces/optim/`) supports:

- **Presets**: V1 (Y-offset), V2 (camber/thickness), V3 (B-spline control points)
- **Multi-objective optimization**: Weighted sum of objectives at different operating points
- **Per-objective settings**: Each objective can have its own alpha/Cl target and Reynolds number
- **Dynamic constraints**: Geometric (thickness, camber, LE radius) and aerodynamic (Cl, Cd, L/D)
- **Mode A/B**: 2D-only or 3D-coupled optimization with induced angle correction
- **Configurable bounds**: Separate Y and X bounds scales, X movement chord range

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  fl5-app (Qt GUI)                                           │
│  ├── modules/xdirect/  - 2D airfoil analysis (XDirect)      │
│  ├── modules/xplane/   - 3D wing/plane analysis             │
│  ├── modules/xsail/    - Sail analysis                      │
│  └── interfaces/optim/ - Optimization dialogs & PSO tasks   │
│      ├── optimizationpanel.cpp/h  - Main optimization UI    │
│      ├── psotaskfoil.cpp/h        - Foil PSO task           │
│      ├── psotaskplane.cpp/h       - Plane PSO task          │
│      └── particle.cpp/h           - PSO particle            │
├─────────────────────────────────────────────────────────────┤
│  fl5-lib (Core C++ Library)                                 │
│  ├── api/         - Public API headers, splines, vectors    │
│  ├── geom/        - Geometry operations, meshing            │
│  ├── objects2d/   - Foil, Polar, OpPoint, XFoilTask        │
│  ├── objects3d/   - Wing, Plane, Body, Panel                │
│  └── analysis3d/  - 3D panel methods, LLT, VLM              │
├─────────────────────────────────────────────────────────────┤
│  XFoil-lib (Aerodynamic Solver)                             │
│  └── Pure C++ translation of XFOIL (no Qt dependencies)     │
└─────────────────────────────────────────────────────────────┘
```

## Build Commands

```bash
# Full build sequence (order matters - dependencies must build first)
qmake6 XFoil-lib/XFoil-lib.pro && make -C XFoil-lib
qmake6 fl5-lib/fl5-lib.pro && make -C fl5-lib
qmake6 flow5.pro && make

# Quick rebuild (after initial build)
make -j4

# Headless optimization API test
OPENBLAS_NUM_THREADS=1 QT6_INCLUDE_DIR=/usr/include/x86_64-linux-gnu/qt6 \
  QT6_LIB_DIR=/usr/lib/x86_64-linux-gnu XFOIL_LIB_DIR=$(pwd)/XFoil-lib \
  FL5_LIB_DIR=$(pwd)/fl5-lib API_examples/foiloptimize/run_test.sh
```

**Note:** `OPENBLAS_NUM_THREADS=1` is required to prevent segfaults in spline solving.

## Key Classes & Data Flow

**Foil Optimization Path:**
```
XDirect menu → OptimFoilDlg → OptimizationPanel → PSOTaskFoil → XFoilTask
                                    ↓
                              ObjectiveSpec[]     - Multi-objective definitions
                              Constraints         - Geometric/aerodynamic limits
                              Particle[]          - PSO swarm
```

**Core Types:**
- `Foil` (`objects2d/foil.h`): Airfoil shape with base nodes → spline → mesh pipeline
- `Polar` (`objects2d/analysis2d/polar.h`): Operating condition container (Re, Mach, NCrit)
- `XFoilTask` (`objects2d/analysis2d/xfoiltask.h`): Thread-safe XFoil solver wrapper
- `PSOTaskFoil` (`fl5-app/interfaces/optim/psotaskfoil.h`): PSO optimization for foils
- `ObjectiveSpec` (`psotaskfoil.h`): Multi-objective definition (type, target, Re, weight)
- `OptimizationPanel` (`optimizationpanel.h`): Main optimization UI with dynamic rows

## Subagent Usage

**Always use subagents for repetitive tasks to save context and improve efficiency:**

```
# Code exploration and searching
Task tool with subagent_type=Explore  - For finding files, understanding codebase structure

# Build and test
Task tool with subagent_type=Bash     - For running make, test scripts

# Git operations
Task tool with subagent_type=Bash     - For commits, status, push
```

**When to use subagents:**
- Searching for code patterns across the codebase
- Running build commands (`make -j4`)
- Running test suites (`API_examples/foiloptimize/run_test.sh`)
- Git operations (status, diff, commit, push)
- Any task that might need multiple iterations

## Issue Tracking

Uses **bd (beads)** for issue tracking:
```bash
bd ready              # Find unblocked work
bd show <id>          # Read issue description before starting work
bd create "Title" --type task --priority 2
bd close <id>         # Complete work
bd sync               # Sync at session end (mandatory)
```

**Always run `bd show <id>` to read issue descriptions before starting work.**

## Global Object Registry

Flow5 uses global object arrays. **API users must call cleanup:**
```cpp
Objects2d::deleteObjects();  // Required on exit
```

Factory pattern for object creation:
```cpp
Foil* foil = foil::makeNacaFoil(...);
Polar* polar = Objects2d::createPolar();
Objects2d::insertPolar(polar);
```

## Coding Conventions

- C++ with Qt conventions: `CamelCase` classes, `m_` prefix for member fields
- Follow existing include ordering and module layout patterns
- New modules/files should use Johan Hedlund in the file header (not Andre Deperrois)
- Qt 6 + C++17 standard
- Linear algebra: OpenBLAS (Linux), MKL (Windows), Accelerate (macOS)

## Testing

No formal unit test framework. For new solver/optimization code:
- Add minimal reproducible examples in `API_examples/`
- Run headless tests before committing: `API_examples/foiloptimize/run_test.sh`
- Run header lint for new files: `scripts/lint_headers.sh upstream/main`

## V1 Foil Optimization: Lessons Learned (LE Geometry)

**Problem**: V1 optimization creates bad leading edge (LE) geometry - loops, self-intersections, concave regions.

**Failed Approaches (DO NOT REPEAT):**

1. **Split spline with CubicSpline (interpolating)** - LE loops
   - Interpolating splines force curve through ALL points → oscillations
   - Natural boundary conditions give no tangent control at LE

2. **Phantom points near LE** - Still loops
   - Adding phantom points 0.1% chord from LE caused spline oscillation
   - Whether using + or - signs, the CubicSpline overshoots

3. **Clamped boundary conditions on CubicSpline** - Still loops
   - Added setClampedStart/End to specify tangent at LE
   - Vertical tangent (0, -1) didn't prevent the loop

4. **BSpline (approximating) for split spline** - Still loops
   - Approximating splines with fewer control points still created LE issues
   - V3 already had this approach and crashes

**Key Insight**: The split-spline approach is fundamentally flawed for this use case. Trying to join two separate splines at the LE always creates discontinuity issues.

**What Actually Works (baseline from before split spline):**
- Direct Y-offset modification: apply offsets directly to base nodes
- Let Foil::initGeometry() handle the spline fitting (single continuous spline)
- Accept that LE geometry may degrade but handle via:
  - Geometry validation (isFoilGeometryValid) to reject bad particles
  - Tighter bounds near LE (reduce how much points can move)
  - m_LEBlendChord to blend optimized shape back toward original near LE

**V3 Status**: Already has B-spline control point approach but crashes - needs debugging, not re-implementing in V1.
