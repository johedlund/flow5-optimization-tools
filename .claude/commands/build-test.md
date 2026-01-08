# Build and Test

Run the full Flow5 build sequence and headless optimization tests.

## Instructions

Execute the following steps in order, stopping on first failure:

### 1. Build XFoil-lib
```bash
cd /home/johe2/optiflow5 && qmake6 XFoil-lib/XFoil-lib.pro && make -j4 -C XFoil-lib
```

### 2. Build fl5-lib
```bash
cd /home/johe2/optiflow5 && qmake6 fl5-lib/fl5-lib.pro && make -j4 -C fl5-lib
```

### 3. Build fl5-app
```bash
cd /home/johe2/optiflow5 && qmake6 flow5.pro && make -j4
```

### 4. Run Headless Optimization Test
```bash
cd /home/johe2/optiflow5 && OPENBLAS_NUM_THREADS=1 API_examples/foiloptimize/run_test.sh
```

## Output Format

Report:
- Build status for each component (pass/fail)
- Test output summary
- Any errors or warnings

If all steps pass, respond with: "Build and tests passed successfully."

If any step fails, stop immediately and report:
- Which step failed
- The error output
- Suggested fix if obvious
