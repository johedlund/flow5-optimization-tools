---
name: new-api-test
description: Scaffold a new headless API test under API_examples/<name>/ matching the geometry_test / xfoilrun / constraint_test pattern (CMakeLists.txt, run_test.sh, .cpp stub with pass/fail summary + non-zero exit on failure). Also registers the test in run_all_tests.sh.
disable-model-invocation: true
---

# Scaffold a new headless API test

Use this when the user wants a new test in the `API_examples/*_test/` pattern. The established convention (verified on `geometry_test`, `constraint_test`, `analysis3d_test`, `xfoilrun`) is:

- CMake project with `Qt6::Core Qt6::Concurrent Qt6::Widgets`, paths via `QT6_INCLUDE_DIR` / `QT6_LIB_DIR` / `XFOIL_LIB_DIR` / `FL5_LIB_DIR` cache vars.
- `run_test.sh` that autodiscovers Qt/XFoil/fl5-lib on Linux, SKIPs cleanly if deps missing, and sets `OPENBLAS_NUM_THREADS=1` at runtime.
- `<name>.cpp` with the GPL header, a `TestResult{name, passed}` struct, a summary block, and `return allPassed ? 0 : 2;`.

## How to invoke

Required argument: `<name>` — snake_case directory name (e.g. `panel_test`, `spline_test`). Must end in `_test`.

Optional argument: `<description>` — one-line purpose for the cpp file's header comment. If omitted, use the name.

Run the scaffold script from the repo root:

```bash
bash .claude/skills/new-api-test/scaffold.sh <name> ["<description>"]
```

## What the script does

1. Creates `API_examples/<name>/` with CMakeLists.txt, run_test.sh (chmod +x), and `<name>.cpp`.
2. CamelCases `<name>` for the CMake project/target name (e.g. `panel_test` → `PanelTest`).
3. Appends `<name>` to the `tests=( … )` array in `API_examples/run_all_tests.sh` if not already present.
4. Prints next steps (add test bodies, run locally, commit).

## After scaffolding

Tell the user:
- The stub has a single always-passing placeholder test — replace with real assertions before committing.
- Verify with: `XFOIL_LIB_DIR=$(pwd)/XFoil-lib FL5_LIB_DIR=$(pwd)/fl5-lib bash API_examples/<name>/run_test.sh`
- Optionally extend `API_examples/<name>/CMakeLists.txt` to pull in additional optim sources (see `geometry_test` for the pattern that links `interfaces/optim/particle.cpp` etc.).

## Lessons-learned reminders to surface

If the new test will call into optimization or XFoil, remind the user of the usual traps from `docs/lessons-learned.md`:
- Always set `OPENBLAS_NUM_THREADS=1` (the generated `run_test.sh` does this — don't remove it).
- Validate foil geometry before XFoil calls.
- Use `OPTIM_PENALTY` for invalid / unconverged results, not zero or NaN.
