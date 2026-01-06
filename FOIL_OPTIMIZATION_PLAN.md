# Foil Optimization in Flow5 — Project Plan (Source of Truth)

This document is the step-by-step plan to deliver a **fully integrated foil optimization workflow** in Flow5 (XDirect UI → optimization task → XFoil evaluation → results + persistence).

Notes:
- The Markdown files under `docs/` are **outdated** and must be treated as **inspiration only**, not as authoritative requirements.
- This plan is intended to be migrated into **Beads (bd)** issue tracking once approved.

---

## 0) Current Status (Updated Jan 2026)

**Completed:**
- **UI Workflow:** `OptimFoilDlg` has a working Run/Cancel/Apply Best workflow.
- **Solver:** OpenBLAS crash fixed (synchronous fallback). `PSOTaskFoil` supports Preset V1 (Y-only) and V2 (Camber/Thickness).
- **Integration:** "Apply Best" successfully inserts the optimized foil into the project.
- **Testing:** Headless test harness (`API_examples/foiloptimize`) covers happy paths, invalid geometry, and unconverged states.

**Remaining Focus:**
- **Persistence:** Saving the optimized foil to the project file (`optiflow5-2xs.1.1`).
- **Objectives:** UI for selecting objectives (currently hardcoded to Cl) (`optiflow5-a08`).
- **Performance/Robustness:** Caching results (`optiflow5-hmi.5`) and audit/watchdogs (`optiflow5-dr4`).
- **Polish:** UX improvements and guardrails (`optiflow5-2xs.2`).

---

## 1) Definition of Done (Project-Level)

The foil optimization feature is “done” when:
- A user can select a foil + polar, choose a variable preset + objectives, and run optimization from XDirect.
- The run can be **cancelled**, progress is visible, and results are inspectable (best, history, pareto if multi-objective).
- The best result can be **applied** (create/insert a new foil, update views), and optionally **saved**.
- The system handles common failure modes (XFoil non-convergence, invalid geometry) without crashes and with clear feedback.
- There is a repeatable headless test/smoke run under `API_examples/` that exercises the new code path and is run before commits.

---

## 2) Guiding Architecture (Keep This Stable)

Keep responsibilities clear:
- **UI layer (`fl5-app`)**: selection, parameter entry, progress display, result presentation, apply/save actions.
- **Optimization task (`fl5-app/interfaces/optim`)**: variable encoding, objective definition, calling XFoil evaluation, penalties, caching.
- **Solver layer (`fl5-lib` + `XFoil-lib`)**: foil geometry + XFoilTask evaluation.

Constraints:
- Optimization task code should be as **UI-agnostic** as practical (callable from API examples).
- Always respect Flow5’s global object registry rules (API users must call `globals::deleteObjects()` on exit).

---

## 3) Step-by-Step Delivery Plan

### Phase A — Proof of Concept → Robust Fitness Evaluation (Headless First)

1. **Codify the “fitness contract”** [DONE]
   - `OPTIM_PENALTY` defined and used for invalid/unconverged states.

2. **Make `PSOTaskFoil::calcFitness()` robust** [DONE]
   - Handles invalid geometry and XFoil errors.
   - Fixed OpenBLAS threading crash.

3. **Stabilize headless testing** [DONE]
   - `run_test.sh` covers multiple scenarios (Happy, Sad, Unconverged, V2).

### Phase B — Variable Presets + Constraints (Make Optimization “Safe”)

4. **Finalize v1 variable preset: y-only base nodes** [DONE]
   - Implemented with fixed LE/TE.

5. **Add variable preset selection plumbing** [DONE]
   - `PresetType` enum and UI combo box implemented.

6. **Add at least one additional preset** [DONE]
   - V2 (Camber/Thickness) implemented.

### Phase C — Optimization Loop Integration (Non-UI + API)

7. **Run PSO end-to-end headlessly** [DONE]
   - Confirmed via `Test 3: Full Headless PSO Run`.

8. **Performance + determinism**
   - Add optional caching of evaluations (hash particle position → metrics). [OPEN: `optiflow5-hmi.5`]
   - Gate multi-threading if XFoil evaluation is not thread-safe in practice. [DONE: Synchronous fallback implemented]

### Phase D — UI Workflow (OptimFoilDlg → Real User Experience)

9. **Complete the dialog workflow** [DONE]
   - Run/Cancel/Apply Best implemented.
   - **Remaining:** Objective selection UI (`optiflow5-a08`).

10. **Progress + logging** [DONE]
   - Progress bar and status label implemented.

11. **Results presentation** [PARTIAL]
   - Basic "Best Cl" message box.
   - **Todo:** Better results summary or pareto visualization.

### Phase E — Project Integration + Persistence

12. **Apply result into Flow5 project** [DONE]
   - `XDirect::onFoilCreated` inserts new foil and updates views.

13. **Save/serialize**
   - Ensure the optimized foil (and any produced polars/op points) are saved with the project. [OPEN: `optiflow5-2xs.1.1`]

### Phase F — Hardening, Quality Gates, and Cleanup

14. **Failure-mode tests + regressions** [PARTIAL]
   - Cancellation test (`optiflow5-hmi.3.4`) still needs work/verification in GUI context.

15. **Polish + guardrails** [OPEN: `optiflow5-2xs.2`]
   - Validate inputs and show actionable error messages.

### Phase G — Foilinizer Vision (Integrated Design Tools)
- [x] Fluid/Reynolds Calculator: Integrated tool to calculate Re based on speed/chord/fluid properties. [2026-01-05]
- [x] Beam Calculator Tool: Structural tool to estimate spar/skin requirements for wings. [2026-01-05]
- [x] Cavitation Analysis & Constraint: Add cavitation limits for hydrofoil optimization. [2026-01-05]
- [x] Enhanced Optimization Progress Dialog: Live plots of fitness and best foil shape. [2026-01-05]

### Phase H — Foilinizer Workflow Adoption (Advanced Optimization)

Based on the analysis of the `Foilinizer` repository, we will adopt its robust optimization workflow directly into Flow5's C++ architecture. We leverage Flow5's native `CubicSpline` geometry to ensure basic continuity ($C^2$), reducing the need for manual curvature checks.

18. **Advanced Constraints Engine**
   - Implement "Wiggliness" constraint (integral of curvature squared) to prevent spline oscillations.
   - Implement Geometric Constraints (LE Radius, TE Thickness, Max t/c).
   - Implement Structural Constraints (Min Section Modulus, Local t/c).
   - Note: Curvature continuity is intrinsically handled by Flow5's spline architecture.

19. **Objective Configuration UI**
   - Allow selecting objective type: Min CD, Max L/D.
   - Allow selecting operating point: "At Target CL" (solve for alpha) vs "At Fixed Alpha".
   - Support "Solve for Flap" mode.

20. **Geometry Resampling & Bounds**
   - Implement control point resampling (reduce variables for optimization, resample to high-res for final).
   - Implement bounds scaling logic (allow user to scale the search space around the initial foil).

### Phase I — Mode B (3D Panel Coupling + Induced AoA)

Mode B integrates Flow5’s 3D panel analysis into the optimization loop to account for induced angle of attack at a selected wing section.

21. **Define Mode B scope + data flow**
    - Identify required inputs: wing, section index, global AoA/CL target, flight conditions, and panel analysis settings.
    - Define how induced AoA is computed and applied to the 2D XFoil evaluation (`alpha_eff = alpha_global - alpha_induced`).
    - Document fallback behavior if the 3D analysis fails or returns invalid data.

22. **3D analysis adapter (induced AoA extractor)**
    - Implement a reusable adapter/service that runs panel analysis and returns induced AoA at the selected section.
    - Support a baseline configuration for other sections and the ability to inject the candidate section geometry.

23. **Optimization loop integration**
    - Extend the optimization task to call the 3D adapter per particle (or per iteration) and evaluate the candidate foil at `alpha_eff`.
    - Ensure the 3D step is cancellable and error-aware (penalty path on failure).

24. **Performance + caching strategy**
    - Add caching for repeated 3D states (wing geometry + conditions → induced AoA).
    - Add user controls for evaluation cadence (e.g., run 3D every N iterations).

25. **UI integration (Mode B)**
    - Add mode toggle (Mode A vs Mode B), wing + section selectors, and analysis condition inputs.
    - Surface 3D-stage progress and warnings about runtime costs.

26. **Testing**
    - Headless test for induced AoA extraction on a known wing.
    - Integration test for the Mode B loop (3D → induced AoA → XFoil evaluation).
    - Failure-mode test when 3D analysis is invalid/unavailable.

27. **Validation + acceptance**
    - Compare Mode A vs Mode B outputs for a known wing/section.
    - Define acceptance criteria and record expected behavior in tests/logs.

### Phase J — Finalize Beads tracking + closeout

28. **Finalize Beads tracking + closeout**
   - Keep backlog tidy.

---

## 4) Proposed Beads Breakdown (To Create After Approval)

When approved, create Beads issues roughly along these lines (titles can be adjusted):

**Feature**
- “FoilOptim: Robust calcFitness penalties + polar-type handling”
- “FoilOptim: Variable preset v1 (y-only base nodes) + constraints”
- “FoilOptim: Variable preset selection plumbing (UI + task)”
- “FoilOptim: Add preset v2 (camber/thickness or coupled y)”
- “FoilOptim: Headless PSO run harness (deterministic small run)”
- “FoilOptim: OptimFoilDlg run/cancel/progress/results workflow”
- “FoilOptim: Apply Best → create/insert optimized foil”
- “FoilOptim: Save optimized foil in project”

**Test**
- “FoilOptim Test: baseline converged fitness (finite metrics)”
- “FoilOptim Test: invalid geometry → penalty path”
- “FoilOptim Test: XFoil unconverged → penalty path”
- “FoilOptim Test: cancellation path”

**Bug**
- “FoilOptim Bug: OpenBLAS parallel dgetrf segfault in spline solve (mitigate/document)”
- “FoilOptim Bug: validate polar selection and objective targets”

**Todo/Chore**
- “FoilOptim: caching of evaluations (hash particle positions)”
- “FoilOptim: multi-threading safety audit (XFoilTask globals)”
- “FoilOptim: UI polish + user feedback improvements”
