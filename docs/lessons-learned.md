# Foil Optimization Lessons Learned

This document captures hard-won lessons from implementing foil optimization in Flow5. **Read this before modifying optimization code.**

## V1 Foil Optimization: Leading Edge Geometry Issues

### The Problem

V1 optimization creates bad leading edge (LE) geometry: loops, self-intersections, concave regions. These cause XFoil failures and invalid foils.

### Failed Approaches (DO NOT REPEAT)

#### 1. Split spline with CubicSpline (interpolating)
**Result**: LE loops
**Why it failed**:
- Interpolating splines force curve through ALL points, causing oscillations
- Natural boundary conditions give no tangent control at LE

#### 2. Phantom points near LE
**Result**: Still loops
**Why it failed**:
- Adding phantom points 0.1% chord from LE caused spline oscillation
- Whether using + or - signs, the CubicSpline overshoots

#### 3. Clamped boundary conditions on CubicSpline
**Result**: Still loops
**Why it failed**:
- Added `setClampedStart/End` to specify tangent at LE
- Vertical tangent (0, -1) didn't prevent the loop

#### 4. BSpline (approximating) for split spline
**Result**: Still loops
**Why it failed**:
- Approximating splines with fewer control points still created LE issues
- V3 already had this approach and crashes

### Key Insight

The split-spline approach is **fundamentally flawed** for this use case. Trying to join two separate splines at the LE always creates discontinuity issues.

### What Actually Works

The baseline approach (before split spline) works:

1. **Direct Y-offset modification**: Apply offsets directly to base nodes
2. **Single continuous spline**: Let `Foil::initGeometry()` handle spline fitting
3. **Accept LE degradation**: Handle via:
   - Geometry validation (`isFoilGeometryValid`) to reject bad particles
   - Tighter bounds near LE (reduce how much points can move)
   - `m_LEBlendChord` to blend optimized shape back toward original near LE

### V3 Status

V3 has a B-spline control point approach but crashes. It needs debugging, not re-implementing in V1.

## OpenBLAS Threading Issues

### The Problem

Segfaults in `dgetrf`, `dgesv`, or spline solving when running optimization.

### Root Cause

OpenBLAS defaults to parallel execution. When combined with Qt's threading or nested parallelism in optimization, this causes crashes.

### The Fix

**Always** set `OPENBLAS_NUM_THREADS=1` before any linear algebra operations:

```bash
export OPENBLAS_NUM_THREADS=1
```

This is enforced in:
- `run_test.sh`
- All slash commands
- CI checks

### Why This Works

Single-threaded OpenBLAS avoids race conditions in the BLAS library. The performance loss is acceptable for the workload size in foil optimization.

## XFoil Non-Convergence

### The Problem

XFoil hangs or returns garbage values for certain foil shapes or operating conditions.

### Causes

1. **Extreme angles of attack**: Beyond XFoil's convergence range
2. **Invalid Reynolds numbers**: Too low or negative
3. **Bad foil geometry**: Self-intersecting, too thin, or extreme curvature

### The Fix

1. **Validate before calling XFoil**:
   - Check foil geometry with `isFoilGeometryValid()`
   - Verify Reynolds and alpha ranges

2. **Use penalty for failures**:
   - Return `OPTIM_PENALTY` for unconverged states
   - Don't let the optimizer chase invalid solutions

3. **Timeout protection**:
   - Set reasonable iteration limits in XFoil
   - Abort if no convergence

## General Lessons

1. **Test headless first**: GUI issues compound solver issues
2. **Single-thread debugging**: Isolate threading bugs from solver bugs
3. **Validate geometry aggressively**: Bad input = bad output
4. **Document failures**: This file exists because we learned the hard way
