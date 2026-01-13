# Flow5 Quick Reference

## Session Workflow

```bash
# Start session
/session-start

# Work on issue
bd show <id>              # Read issue before starting
# ... make changes ...
/build                    # Quick rebuild
/test                     # Run tests

# Commit
/commit-with-tests "message"

# End session
/ci-check                 # Verify everything
git pull --rebase && bd sync && git push
```

## Build Commands

| Task | Command |
|------|---------|
| Quick rebuild | `make -j4` |
| Full rebuild | `qmake6 flow5.pro && make -j4` |
| XFoil-lib | `qmake6 XFoil-lib/XFoil-lib.pro && make -C XFoil-lib` |
| fl5-lib | `qmake6 fl5-lib/fl5-lib.pro && make -C fl5-lib` |
| Run tests | `OPENBLAS_NUM_THREADS=1 API_examples/foiloptimize/run_test.sh` |

## Slash Commands

| Command | When to Use |
|---------|-------------|
| `/session-start` | Beginning of each session |
| `/build` | After code changes |
| `/build-test` | Before committing |
| `/test` | Verify optimization works |
| `/debug` | Investigating crashes |
| `/ci-check` | Before pushing |

## Beads (Issue Tracking)

```bash
bd ready                  # Find work to do
bd show <id>              # Read issue details
bd create "Title"         # New issue
bd close <id>             # Mark done
bd sync                   # Sync with git (required before push)
bd stats                  # Overview
```

## Key Directories

| Path | Contents |
|------|----------|
| `fl5-app/` | Qt GUI application |
| `fl5-lib/` | Core C++ library |
| `XFoil-lib/` | Aerodynamic solver |
| `fl5-app/interfaces/optim/` | Optimization UI & PSO |
| `API_examples/foiloptimize/` | Headless tests |
| `docs/` | Specs and documentation |

## Key Files

| File | Purpose |
|------|---------|
| `fl5-lib/objects2d/foil.h` | Foil geometry class |
| `fl5-app/interfaces/optim/psotaskfoil.cpp` | PSO optimization task |
| `fl5-app/interfaces/optim/optimizationpanel.cpp` | Optimization UI |
| `fl5-lib/api/cubicspline.h` | Spline implementation |
| `FOIL_OPTIMIZATION_PLAN.md` | Project roadmap |

## Environment Variables

```bash
# REQUIRED for all tests/optimization
export OPENBLAS_NUM_THREADS=1

# Git identity (set by /session-start)
export GIT_AUTHOR_NAME="Claude Agent"
export GIT_AUTHOR_EMAIL="claude@agent.flow5"
export GIT_COMMITTER_NAME="Claude Agent"
export GIT_COMMITTER_EMAIL="claude@agent.flow5"
```

## Troubleshooting

### Segfault in spline/dgetrf
```bash
# Cause: OpenBLAS threading
# Fix: Ensure this is set
export OPENBLAS_NUM_THREADS=1
```

### XFoil hangs or garbage output
```
Cause: Invalid foil geometry or extreme alpha/Re
Fix: Check isFoilGeometryValid() before XFoil calls
     Use OPTIM_PENALTY for unconverged states
```

### LE geometry loops (optimization)
```
Cause: Split-spline approach is fundamentally broken
Fix: Use direct Y-offset modification
     See docs/lessons-learned.md for details
```

### Build fails after pull
```bash
# Clean rebuild
make clean
qmake6 flow5.pro
make -j4
```

### Tests pass locally, fail in CI
```bash
# Run full CI check
/ci-check
```

## Optimization Presets

| Preset | Variables | Status |
|--------|-----------|--------|
| V1 | Y-offset on base nodes | Working |
| V2 | Camber/thickness | Working |
| V3 | B-spline control points | Crashes (needs debug) |

## Code Patterns

### Create a foil
```cpp
Foil* foil = foil::makeNacaFoil(...);
Objects2d::insertFoil(foil);
```

### Run XFoil analysis
```cpp
XFoilTask task;
task.setFoil(foil);
task.setPolar(polar);
task.run();  // Check return value!
```

### Cleanup (required)
```cpp
Objects2d::deleteObjects();  // Call on exit
```

## Git Conventions

- Commit messages: imperative mood ("Add feature" not "Added feature")
- Reference beads: `Closes: optiflow5-xxx`
- Always run tests before commit
- Push before ending session
