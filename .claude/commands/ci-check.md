# CI Check

Run full CI-equivalent checks before pushing. Use this before `git push`.

## Instructions

Execute all checks in sequence:

### 1. Full Build (Clean)
```bash
cd /home/johe2/optiflow5
qmake6 XFoil-lib/XFoil-lib.pro && make -j4 -C XFoil-lib
qmake6 fl5-lib/fl5-lib.pro && make -j4 -C fl5-lib
qmake6 flow5.pro && make -j4
```

### 2. Header Lint
```bash
cd /home/johe2/optiflow5 && scripts/lint_headers.sh HEAD~1
```

### 3. Headless Tests
```bash
cd /home/johe2/optiflow5 && OPENBLAS_NUM_THREADS=1 API_examples/foiloptimize/run_test.sh
```

### 4. Check Unpushed Commits
```bash
cd /home/johe2/optiflow5 && git log origin/main..HEAD --oneline
```

### 5. Beads Sync Status
```bash
cd /home/johe2/optiflow5 && bd stats
```

## Output Summary

```
CI CHECK RESULTS
================
Build:      [PASS/FAIL]
Lint:       [PASS/FAIL]
Tests:      [PASS/FAIL]
Unpushed:   X commits
Beads:      X open issues

Ready to push: [YES/NO]
```

## If All Pass

Suggest:
```bash
git pull --rebase && bd sync && git push
```

## If Any Fail

Do NOT push. Report failures and suggest fixes.
