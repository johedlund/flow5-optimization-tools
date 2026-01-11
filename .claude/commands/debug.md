# Debug with GDB

Launch the debugger for Flow5 or headless tests.

## Arguments

- `$ARGUMENTS` - Optional: "app" for GUI, "test" for headless tests (default: test)

## Instructions

### For headless tests (default)
```bash
cd /home/johe2/optiflow5
OPENBLAS_NUM_THREADS=1 gdb -ex "run" -ex "bt full" --args API_examples/foiloptimize/build/foiloptimize_test
```

### For GUI application
```bash
cd /home/johe2/optiflow5
OPENBLAS_NUM_THREADS=1 gdb -ex "run" ./fl5-app/flow5
```

## On Crash

If a crash occurs, capture:
1. Full backtrace: `bt full`
2. Thread info: `info threads`
3. Local variables: `info locals`

## Common Breakpoints

```gdb
# Spline issues
break CubicSpline::solve
break BSpline::evaluate

# XFoil entry
break XFoilTask::run

# Optimization
break PSOTaskFoil::calcFitness
```

Report the crash location, backtrace, and any relevant variable values.
