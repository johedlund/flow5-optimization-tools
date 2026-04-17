---
name: foil-validate
description: Validate foil optimization code against known geometry pitfalls, OpenBLAS threading issues, and XFoil convergence traps documented in lessons-learned.md
user-invocable: true
---

# Foil Validation Skill

Validate optimization code changes against the project's documented failure modes. Run this before committing changes to `interfaces/optim/`, `objects2d/`, or `XFoil-lib/`.

## Validation Checklist

### 1. Leading Edge Geometry (CRITICAL)

Read `docs/lessons-learned.md` and check for:

- **No split-spline LE approach**: Search changed files for any pattern that splits the foil at LE into two separate splines. This is fundamentally flawed — reject immediately.
- **No interpolating CubicSpline at LE**: Check for `CubicSpline` usage near LE points. Approximating B-splines or direct Y-offset are the only safe approaches.
- **Geometry validation present**: Any code path that modifies foil geometry must call `isFoilGeometryValid()` before passing to XFoil.

Search patterns:
```
grep -rn "splitSpline\|split_spline\|CubicSpline.*LE\|CubicSpline.*leading" fl5-lib/ fl5-app/interfaces/optim/
```

### 2. OpenBLAS Threading

- **OPENBLAS_NUM_THREADS=1**: Verify all test scripts, run configurations, and CI steps set this environment variable. Missing it causes segfaults in `dgetrf`/`dgesv`.
- Check: `grep -rn "OPENBLAS_NUM_THREADS" API_examples/ scripts/ .github/ .claude/`

### 3. XFoil Convergence Guards

- **Penalty for failures**: Optimization fitness functions must return `OPTIM_PENALTY` for unconverged XFoil results, not zero or NaN.
- **Input validation**: Reynolds > 0, alpha within reasonable range, foil geometry validated.
- **Timeout protection**: XFoil iteration limits should be set.

Search patterns:
```
grep -rn "OPTIM_PENALTY\|isConverged\|isFoilGeometryValid" fl5-app/interfaces/optim/ fl5-lib/objects2d/
```

### 4. Build & Test Verification

Run the full validation:
```bash
cd /home/johe2/optiflow5
make -j4 2>&1

OPENBLAS_NUM_THREADS=1 QT6_INCLUDE_DIR=/usr/include/x86_64-linux-gnu/qt6 \
  QT6_LIB_DIR=/usr/lib/x86_64-linux-gnu \
  XFOIL_LIB_DIR=$(pwd)/XFoil-lib \
  FL5_LIB_DIR=$(pwd)/fl5-lib \
  API_examples/foiloptimize/run_test.sh 2>&1
```

## Output

Report results as:
```
## Foil Validation Report

| Check | Status | Details |
|-------|--------|---------|
| No split-spline LE | ✅/❌ | ... |
| Geometry validation | ✅/❌ | ... |
| OPENBLAS_NUM_THREADS | ✅/❌ | ... |
| XFoil convergence guards | ✅/❌ | ... |
| Build | ✅/❌ | ... |
| Headless tests | ✅/❌ | ... |
```
