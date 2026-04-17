#!/usr/bin/env bash
set -uo pipefail

# Run all Flow5 headless tests.
# Exit code: 0 if all pass, 1 if any fail.
#
# Required environment:
#   OPENBLAS_NUM_THREADS=1  (prevents segfaults in spline solving)
#
# Optional environment:
#   QT6_INCLUDE_DIR, QT6_LIB_DIR, XFOIL_LIB_DIR, FL5_LIB_DIR

export OPENBLAS_NUM_THREADS=1

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
failed=0
passed=0
skipped=0

tests=(
    "foiloptimize"
    "xfoilrun"
    "geometry_test"
    "analysis3d_test"
    "constraint_test"
    "induced_aoa_test"
)

echo "============================================"
echo "  Flow5 Test Suite"
echo "============================================"
echo ""

for test in "${tests[@]}"; do
    script="${root_dir}/API_examples/${test}/run_test.sh"
    if [[ ! -x "${script}" ]]; then
        echo "[SKIP] ${test} (no run_test.sh)"
        skipped=$((skipped + 1))
        continue
    fi

    echo "--- ${test} ---"
    if bash "${script}"; then
        echo "[PASS] ${test}"
        passed=$((passed + 1))
    else
        echo "[FAIL] ${test}"
        failed=$((failed + 1))
    fi
    echo ""
done

echo "============================================"
echo "  Results: ${passed} passed, ${failed} failed, ${skipped} skipped"
echo "============================================"

if [[ ${failed} -gt 0 ]]; then
    exit 1
fi
exit 0
