#!/usr/bin/env bash
# Scaffold a new headless API test matching the geometry_test / constraint_test pattern.
#
# Usage: scaffold.sh <name> ["<description>"]
#   <name>         snake_case directory name, e.g. panel_test (should end in _test)
#   <description>  one-line purpose for the cpp header (optional)

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <name_test> [\"<description>\"]" >&2
  exit 2
fi

name="$1"
description="${2:-${name//_/ } validation}"

# Resolve repo root (the skill lives at .claude/skills/new-api-test/)
skill_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${skill_dir}/../../.." && pwd)"

if [[ ! -f "${repo_root}/fl5-lib/fl5-lib.pro" ]]; then
  echo "error: expected to run inside an optiflow5 checkout (no fl5-lib.pro at ${repo_root})" >&2
  exit 2
fi

target_dir="${repo_root}/API_examples/${name}"
if [[ -e "${target_dir}" ]]; then
  echo "error: ${target_dir} already exists — refusing to overwrite" >&2
  exit 2
fi

# Derive CamelCase project/exe name from snake_case: panel_test -> PanelTest
project="$(python3 -c "import sys; print(''.join(w.capitalize() for w in sys.argv[1].split('_')))" "${name}")"

echo "==> scaffolding ${target_dir}"
echo "    project/exe name : ${project}"
echo "    description      : ${description}"

mkdir -p "${target_dir}"

subst() {
  local in="$1" out="$2"
  sed \
    -e "s|__PROJECT__|${project}|g" \
    -e "s|__NAME__|${name}|g" \
    -e "s|__SRC__|${name}.cpp|g" \
    -e "s|__DESCRIPTION__|${description}|g" \
    "${in}" > "${out}"
}

subst "${skill_dir}/template/CMakeLists.txt" "${target_dir}/CMakeLists.txt"
subst "${skill_dir}/template/run_test.sh"    "${target_dir}/run_test.sh"
subst "${skill_dir}/template/test.cpp"       "${target_dir}/${name}.cpp"
chmod +x "${target_dir}/run_test.sh"

# Register in run_all_tests.sh if not already present.
runner="${repo_root}/API_examples/run_all_tests.sh"
if [[ -f "${runner}" ]] && ! grep -qE "^ *\"${name}\"" "${runner}"; then
  python3 - "${runner}" "${name}" <<'PY'
import re, sys
runner, name = sys.argv[1], sys.argv[2]
with open(runner) as f:
    text = f.read()
m = re.search(r'(tests=\(\n)(.*?)(\n\))', text, re.DOTALL)
if not m:
    sys.stderr.write("warning: could not locate tests=( ... ) block in run_all_tests.sh\n")
    sys.exit(0)
entries = m.group(2).rstrip("\n")
new_entries = entries + f'\n    "{name}"'
updated = text[:m.start(2)] + new_entries + text[m.end(2):]
with open(runner, "w") as f:
    f.write(updated)
PY
  echo "==> registered ${name} in API_examples/run_all_tests.sh"
fi

cat <<NEXT
==> done

next steps:
  1. Edit ${target_dir}/${name}.cpp — replace the placeholder test with real assertions.
  2. Verify locally:
       XFOIL_LIB_DIR=\$(pwd)/XFoil-lib FL5_LIB_DIR=\$(pwd)/fl5-lib \\
         bash API_examples/${name}/run_test.sh
  3. Commit: API_examples/${name}/ and the run_all_tests.sh update.
NEXT
