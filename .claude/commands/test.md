# Run Headless Tests

Run the foil optimization headless test suite.

## Instructions

```bash
cd /home/johe2/optiflow5 && OPENBLAS_NUM_THREADS=1 API_examples/foiloptimize/run_test.sh
```

## Output

Report:
- Test pass/fail status
- Key metrics (fitness values, iteration counts)
- Any errors or warnings

If test crashes or hangs, note the failure mode.
