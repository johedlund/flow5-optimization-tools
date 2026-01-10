# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Git Identity

**Run this at session start** to track contributions:
```bash
export GIT_AUTHOR_NAME="Claude Agent" GIT_AUTHOR_EMAIL="claude@agent.flow5" GIT_COMMITTER_NAME="Claude Agent" GIT_COMMITTER_EMAIL="claude@agent.flow5"
```

## Project Overview

Flow5 is a potential flow solver (aerodynamic analysis) with built-in pre/post-processing. It's version 7 of the legacy xflr5 project, used for preliminary design of wings, planes, hydrofoils, and sails.

## Build Commands

```bash
# Full build sequence (order matters - dependencies must build first)
qmake6 XFoil-lib/XFoil-lib.pro && make -C XFoil-lib
qmake6 fl5-lib/fl5-lib.pro && make -C fl5-lib
qmake6 flow5.pro && make

# Or from Qt Creator: open flow5.pro

# Headless optimization API test
OPENBLAS_NUM_THREADS=1 QT6_INCLUDE_DIR=/usr/include/x86_64-linux-gnu/qt6 \
  QT6_LIB_DIR=/usr/lib/x86_64-linux-gnu XFOIL_LIB_DIR=$(pwd)/XFoil-lib \
  FL5_LIB_DIR=$(pwd)/fl5-lib API_examples/foiloptimize/run_test.sh
```

**Note:** `OPENBLAS_NUM_THREADS=1` is required to prevent segfaults in spline solving.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  fl5-app (Qt GUI)                                           │
│  ├── modules/xdirect/  - 2D airfoil analysis                │
│  ├── modules/xplane/   - 3D wing analysis                   │
│  └── interfaces/optim/ - Optimization dialogs & PSO tasks   │
├─────────────────────────────────────────────────────────────┤
│  fl5-lib (Core C++ Library)                                 │
│  ├── api/         - Public API headers                      │
│  ├── geom/        - Geometry operations, splines            │
│  ├── objects2d/   - Foil, Polar, OpPoint, XFoilTask        │
│  └── analysis3d/  - 3D panel methods                        │
├─────────────────────────────────────────────────────────────┤
│  XFoil-lib (Aerodynamic Solver)                             │
│  └── Pure C++ translation of XFOIL (no Qt dependencies)     │
└─────────────────────────────────────────────────────────────┘
```

## Key Classes & Data Flow

**Foil Optimization Path:**
```
XDirect menu → OptimFoilDlg → PSOTaskFoil → XFoilTask::calcFitness()
                                  ↓
                              Foil geometry (base nodes)
                              Polar (Re, Mach, NCrit)
                              Objectives (Cl, Cd, Cl/Cd targets)
```

**Core Types:**
- `Foil` (`objects2d/foil.h`): Airfoil shape with base nodes → spline → mesh pipeline
- `Polar` (`objects2d/analysis2d/polar.h`): Operating condition container (Re, AoA, etc.)
- `XFoilTask` (`objects2d/analysis2d/xfoiltask.h`): Thread-safe XFoil solver wrapper
- `PSOTaskFoil` (`fl5-app/interfaces/optim/psotaskfoil.h`): PSO optimization for foils
- `OptObjective/OptVariable` (`api/optstructures.h`): Optimization problem definition

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

## Issue Tracking

Uses **bd (beads)** for issue tracking:
```bash
bd ready              # Find unblocked work
bd create "Title" --type task --priority 2
bd close <id>         # Complete work
bd sync               # Sync at session end (mandatory)
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
