# Commit with Tests

Run all quality gates before committing changes.

## Arguments

- `$ARGUMENTS` - Commit message (required)

## Instructions

Execute these steps in order. **Stop on first failure.**

### 1. Run Quick Build
```bash
cd /home/johe2/optiflow5 && make -j4 2>&1
```

### 2. Run Header Lint
```bash
cd /home/johe2/optiflow5 && scripts/lint_headers.sh HEAD~1 2>&1
```

### 3. Run Headless Tests
```bash
cd /home/johe2/optiflow5 && OPENBLAS_NUM_THREADS=1 API_examples/foiloptimize/run_test.sh 2>&1
```

### 4. Check Git Status
```bash
cd /home/johe2/optiflow5 && git status
```

### 5. Stage and Commit

If all checks pass:
```bash
cd /home/johe2/optiflow5 && git add -A && git commit -m "$ARGUMENTS"
```

## On Failure

If any step fails:
1. Report which step failed
2. Show the error output
3. Do NOT commit
4. Suggest fix if obvious

## Success Output

Report:
- Build: PASS/FAIL
- Lint: PASS/FAIL
- Tests: PASS/FAIL
- Commit hash (if successful)
