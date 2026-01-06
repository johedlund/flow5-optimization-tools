#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if ! command -v qmake6 >/dev/null 2>&1; then
  echo "ERROR: qmake6 not found. Install Qt6 and try again." >&2
  exit 1
fi

if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
  jobs="${JOBS:-}"
  if [[ -z "${jobs}" ]]; then
    jobs="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
  fi

  qmake6 "${root_dir}/flow5.pro"
  make -j"${jobs}"
fi

export LD_LIBRARY_PATH="${root_dir}/fl5-lib:${root_dir}/XFoil-lib:${LD_LIBRARY_PATH:-}"
export OPENBLAS_NUM_THREADS="${OPENBLAS_NUM_THREADS:-1}"

# Optional: set FORCE_SW_RENDER=1 to avoid EGL/QRhi issues in WSL/VMs.
if [[ "${FORCE_SW_RENDER:-0}" != "0" ]]; then
  export QT_OPENGL="${QT_OPENGL:-software}"
  export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}"
  export QT_WIDGETS_RHI="${QT_WIDGETS_RHI:-0}"
  export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
fi

exec "${root_dir}/fl5-app/flow5" "$@"
