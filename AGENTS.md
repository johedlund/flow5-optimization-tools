# Repository Guidelines

## Issue Tracking

This project uses **bd (beads)** for issue tracking.
Run `bd prime` for workflow context, or install hooks (`bd hooks install`) for auto-injection.

**Quick reference:**
- `bd ready` - Find unblocked work
- `bd create "Title" --type task --priority 2` - Create issue
- `bd close <id>` - Complete work
- `bd sync` - Sync with git (run at session end)

For full workflow details: `bd prime`

## Project Structure & Module Organization
- `fl5-lib/`: core C++ libraries (analysis3d, api, geom, objects2d/3d, xml).
- `fl5-app/`: Qt application (modules `xdirect` for 2D and `xplane` for 3D, `interfaces/optim` for optim UI/tasks).
- `XFoil-lib/`: C++ translation of XFOIL sources.
- `API_examples/`: headless API examples (e.g., XFoilTask usage).
- `docs/`: integration specs and architectural notes.
- `meta/`: project metadata and auxiliary files.

## Architecture & Workflow
- UI (fl5-app) -> core libraries (fl5-lib) -> XFoil-lib solver.
- Optimization tasks live under `fl5-app/interfaces/optim/` and should be UI-agnostic where possible.
- Flow5 uses a global object registry; API users must call `globals::deleteObjects()` on exit.

## Build, Test, and Development Commands
- Build configuration lives in `flow5.pro` (Qt/QMake project).
- Use Qt Creator or qmake with `flow5.pro` (see `README.md` for environment setup).
- API usage patterns are demonstrated in `API_examples/`.
- Typical local builds:
  - `qmake6 XFoil-lib/XFoil-lib.pro && make -C XFoil-lib`
  - `qmake6 fl5-lib/fl5-lib.pro && make -C fl5-lib`
  - `qmake6 flow5.pro && make`
- Foil optimization API test (requires XFoil-lib and fl5-lib built):
  - `QT6_INCLUDE_DIR=/usr/include/x86_64-linux-gnu/qt6 QT6_LIB_DIR=/usr/lib/x86_64-linux-gnu XFOIL_LIB_DIR=/path/to/XFoil-lib FL5_LIB_DIR=/path/to/fl5-lib API_examples/foiloptimize/run_test.sh`
  - The script sets `OPENBLAS_NUM_THREADS=1` to avoid OpenBLAS parallel segfaults in spline solving.

## Coding Style & Naming Conventions
- C++ with Qt conventions: `CamelCase` classes, `m_` prefixes for member fields.
- Follow existing module layout and include ordering patterns.

## Testing Guidelines
- No formal unit tests detected; prefer small deterministic runs in `API_examples/` for new solver paths.
- For new optimization logic, add a minimal reproducible example and log expected outputs.
- For any code change, add or update tests for the new behavior and run them before committing; capture the commands used.

## Commit & Pull Request Guidelines
- Recommendation: use short, imperative summaries (e.g., “Add foil optimization scaffold”).
- PRs: include a concise description, list tests run, and link related issues.

## Landing the Plane (Session Completion)

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create issues for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished work, update in-progress items
4. **PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   git pull --rebase
   bd sync
   git push
   git status  # MUST show "up to date with origin"
   ```
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds
