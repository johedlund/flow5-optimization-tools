# Mode B: 3D Panel Coupling Specification

## Overview

Mode B extends foil optimization to account for **induced angle of attack** from 3D wing analysis. In Mode A, the optimizer assumes the foil operates at a fixed geometric AoA. Mode B corrects this by computing the effective AoA at a specific wing section using Flow5's 3D panel solver.

```
Mode A: alpha_effective = alpha_geometric
Mode B: alpha_effective = alpha_geometric + alpha_induced
```

## Required Inputs

### Wing/Plane Selection
| Input | Type | Source | Notes |
|-------|------|--------|-------|
| Plane | `PlaneXfl*` | Objects3d registry | Selected from existing planes in project |
| Wing | `WingXfl*` | `plane->mainWing()` | Can also be stab/fin |
| Section Index | `int` | UI dropdown | 0 to nSections-1 |

### Flight Conditions
| Input | Type | Default | Notes |
|-------|------|---------|-------|
| Velocity | `double` | from polar | m/s freestream |
| Alpha (geometric) | `double` | UI target | degrees |
| Density | `double` | 1.225 | kg/m³ |
| Viscosity | `double` | 1.5e-5 | m²/s |

### Analysis Settings
| Input | Type | Default | Notes |
|-------|------|---------|-------|
| Panel Method | enum | VLM2 | VLM1, VLM2, Panel, TriLinear |
| Viscous | `bool` | true | Include viscous drag |
| Wake Model | enum | Fixed | Fixed or Vorton wake |

## Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. INITIALIZATION                                               │
├─────────────────────────────────────────────────────────────────┤
│  User selects: Plane → Wing → Section Index                     │
│  User sets: Flight conditions (V, α, ρ, μ)                      │
│  System stores: baseline wing geometry                          │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│ 2. INITIAL 3D ANALYSIS (once at optimization start)             │
├─────────────────────────────────────────────────────────────────┤
│  PlaneTask::run(plane, polar, alpha_geometric)                  │
│  → Inviscid panel solution                                      │
│  → Extract SpanDistribs for target wing                         │
│  → alpha_induced = SpanDistribs.m_Ai[section_index]             │
│  → alpha_effective = alpha_geometric + alpha_induced            │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│ 3. PSO OPTIMIZATION LOOP                                        │
├─────────────────────────────────────────────────────────────────┤
│  For each particle:                                             │
│    a) Generate candidate foil geometry                          │
│    b) Run XFoil at alpha_effective (not alpha_geometric!)       │
│    c) Evaluate fitness (Cl, Cd, constraints)                    │
│    d) Update particle best/swarm best                           │
│                                                                 │
│  Optional: Re-run 3D analysis every N iterations                │
│    - Inject current best foil into wing section                 │
│    - Recompute alpha_induced                                    │
│    - Continue with updated alpha_effective                      │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│ 4. RESULTS                                                      │
├─────────────────────────────────────────────────────────────────┤
│  Best foil optimized for effective operating condition          │
│  Reports: alpha_geometric, alpha_induced, alpha_effective       │
└─────────────────────────────────────────────────────────────────┘
```

## Key Classes

| Class | File | Role |
|-------|------|------|
| `PlaneTask` | `fl5-lib/api/planetask.h` | Orchestrates 3D analysis |
| `SpanDistribs` | `fl5-lib/api/spandistribs.h` | Contains `m_Ai[]` induced angles |
| `PlanePolar` | `fl5-lib/api/polar3d.h` | Analysis case definition |
| `WingXfl` | `fl5-lib/api/wingxfl.h` | Wing geometry, section access |
| `Surface` | `fl5-lib/api/surface.h` | Panel surface, foil references |

## Induced AoA Extraction

```cpp
// After PlaneTask::run() completes:
PlaneOpp* pPOpp = task.planeOppList().back();
SpanDistribs& sd = pPOpp->wingSpanDistribs(iWing);

// Find station closest to target section
int iStation = findStationForSection(pWing, iSection);
double alpha_induced = sd.m_Ai[iStation];  // degrees

// Effective angle for XFoil
double alpha_effective = alpha_geometric + alpha_induced;
```

### Station Mapping

Wing sections don't map 1:1 to span stations. The adapter must:
1. Get section y-position: `ySection = pWing->section(iSection).m_YPosition`
2. Find closest station in `SpanDistribs.m_PtC4[]` (quarter-chord points)
3. Interpolate `m_Ai` if between stations

## Fallback Behavior

When 3D analysis fails, Mode B gracefully degrades to Mode A:

| Failure Mode | Detection | Fallback |
|--------------|-----------|----------|
| No plane selected | `pPlane == nullptr` | Use Mode A (alpha_induced = 0) |
| No polar defined | `pPolar == nullptr` | Use Mode A + warning |
| Analysis diverged | `PlaneTask::run()` returns false | Use Mode A + log error |
| Invalid section index | `iSection >= nSections` | Clamp + warning |
| NaN in results | `!std::isfinite(m_Ai[i])` | Use Mode A + warning |

**Critical:** Never let a 3D failure crash the optimization. Log and continue with Mode A.

## Re-evaluation Cadence

Updating the 3D solution during optimization is expensive. Options:

| Strategy | When | Use Case |
|----------|------|----------|
| Never | Only at start | Fast, small geometry changes |
| Every N iterations | Periodic | Medium geometry changes |
| On improvement | When swarm best improves | Adaptive, recommended |
| Every particle | Each evaluation | Accurate but very slow |

**Recommended default:** Re-evaluate 3D every 10 iterations OR when swarm best improves by >5%.

## UI Controls (Mode B specific)

```
┌─ Mode Selection ──────────────────────────┐
│  ○ Mode A: 2D only (fixed AoA)            │
│  ● Mode B: 3D coupled (induced AoA)       │
├───────────────────────────────────────────┤
│  Plane:    [Sailplane v1        ▼]        │
│  Wing:     [Main Wing           ▼]        │
│  Section:  [3 (y=1.5m)          ▼]        │
├───────────────────────────────────────────┤
│  3D Update: [Every 10 iters     ▼]        │
│  ☑ Show induced AoA in log                │
└───────────────────────────────────────────┘
```

## Implementation Phases

1. **hmi.8.1** (this doc) - Define inputs + data flow ✓
2. **hmi.8.2** - 3D analysis adapter (extract induced AoA)
3. **hmi.8.6** - Headless test: induced AoA extraction
4. **hmi.8.3** - Integrate into optimization loop
5. **hmi.8.7** - Integration test + failure path
6. **hmi.8.4** - Caching + evaluation cadence
7. **hmi.8.5** - UI controls
8. **hmi.8.9** - Invalid output handling
9. **hmi.8.8** - Validation vs Mode A
