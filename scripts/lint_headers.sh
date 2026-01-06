#!/usr/bin/env bash
set -euo pipefail

BASE_REF="${1:-}"

if [[ -z "${BASE_REF}" ]]; then
  if git show-ref --verify --quiet refs/remotes/upstream/main; then
    BASE_REF="upstream/main"
  elif git show-ref --verify --quiet refs/remotes/origin/main; then
    BASE_REF="origin/main"
  else
    echo "error: no base ref found. Provide a base ref, e.g. scripts/lint_headers.sh upstream/main" >&2
    exit 2
  fi
fi

if ! git rev-parse --verify --quiet "${BASE_REF}" >/dev/null; then
  echo "error: base ref '${BASE_REF}' not found" >&2
  exit 2
fi

is_source_file() {
  case "$1" in
    *.c|*.cc|*.cpp|*.cxx|*.h|*.hpp|*.inl|*.qml)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

has_pattern() {
  local pattern="$1"
  local file="$2"

  if command -v rg >/dev/null 2>&1; then
    head -n 40 "$file" | rg -q "$pattern"
  else
    head -n 40 "$file" | grep -q "$pattern"
  fi
}

added_files=()
while IFS= read -r -d '' file; do
  added_files+=("$file")
done < <(git diff --name-only --diff-filter=A -z "${BASE_REF}"...HEAD)

if [[ ${#added_files[@]} -eq 0 ]]; then
  echo "No new files to check."
  exit 0
fi

failures=()
ANDRE_ASCII="Andre Deperrois"
ANDRE_UTF8=$'Andr\303\251 Deperrois'
for file in "${added_files[@]}"; do
  if ! is_source_file "$file"; then
    continue
  fi
  if [[ ! -f "$file" ]]; then
    failures+=("$file (missing in working tree)")
    continue
  fi

  has_johan=false
  has_andre=false

  if has_pattern "Johan Hedlund" "$file"; then
    has_johan=true
  fi
  if has_pattern "$ANDRE_ASCII" "$file" || has_pattern "$ANDRE_UTF8" "$file"; then
    has_andre=true
  fi

  if [[ "$has_johan" == false || "$has_andre" == true ]]; then
    failures+=("$file")
  fi
done

if [[ ${#failures[@]} -ne 0 ]]; then
  echo "Header check failed for new source files (expecting 'Johan Hedlund' and not 'Andre/André Deperrois'):" >&2
  for file in "${failures[@]}"; do
    echo "  - $file" >&2
  done
  exit 1
fi

echo "Header check passed."
