# Analyze Crash

Investigate a crash or segfault in Flow5.

## Arguments

- `$ARGUMENTS` - Description of the crash or reproduction steps

## Instructions

### 1. Gather Context

First, check recent changes that might have caused the crash:
```bash
cd /home/johe2/optiflow5 && git log --oneline -10
```

### 2. Identify Crash Pattern

Look for known crash patterns:

**OpenBLAS crashes** (dgetrf, dgesv, spline solving):
- Verify `OPENBLAS_NUM_THREADS=1` is set
- Check for nested parallelism

**Geometry crashes** (CubicSpline, BSpline, Foil):
- Check foil coordinates for validity
- Look for LE/TE edge cases

**XFoil hangs** (XFoilTask):
- Check Reynolds number and angle of attack ranges
- Verify foil geometry is valid before analysis

### 3. Search Codebase

Search for the crash location in code:
```bash
cd /home/johe2/optiflow5 && grep -rn "CRASH_LOCATION" fl5-lib/ fl5-app/
```

### 4. Review Lessons Learned

Check if this is a known issue pattern from CLAUDE.md (V1 optimization LE geometry issues).

## Output

Provide:
1. **Likely Cause**: Based on pattern matching
2. **Investigation Steps**: What to check next
3. **Suggested Fix**: If determinable
4. **Prevention**: How to avoid recurrence
