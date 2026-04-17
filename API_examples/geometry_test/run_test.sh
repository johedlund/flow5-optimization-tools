#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${root_dir}/API_examples/geometry_test/build"

qt_include="${QT6_INCLUDE_DIR:-}"
qt_lib="${QT6_LIB_DIR:-}"
xfoil_lib="${XFOIL_LIB_DIR:-}"
fl5_lib="${FL5_LIB_DIR:-}"
qt_cmake_dir="${QT6_CMAKE_DIR:-}"

if [[ -z "${qt_include}" ]]; then
  for candidate in /usr/include/qt6 /usr/include/x86_64-linux-gnu/qt6; do
    if [[ -f "${candidate}/QtCore/QCoreApplication" ]]; then
      qt_include="${candidate}"
      break
    fi
  done
fi

if [[ -z "${qt_include}" || ! -f "${qt_include}/QtCore/QCoreApplication" ]]; then
  echo "SKIP: Qt6 headers not found (set QT6_INCLUDE_DIR/QT6_LIB_DIR to run)."
  exit 0
fi

if [[ -z "${qt_lib}" ]] && command -v qmake6 >/dev/null 2>&1; then
  qt_lib="$(qmake6 -query QT_INSTALL_LIBS)"
fi

if [[ -z "${qt_cmake_dir}" && -n "${qt_lib}" && -d "${qt_lib}/cmake/Qt6" ]]; then
  qt_cmake_dir="${qt_lib}/cmake/Qt6"
fi

lib_xfoil=""
lib_fl5=""
for candidate in "${xfoil_lib}" "${fl5_lib}" /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib64; do
  if [[ -z "${lib_xfoil}" ]]; then
    if [[ -f "${candidate}/libXFoil.so" || -f "${candidate}/libXFoil.a" ]]; then
      lib_xfoil="${candidate}"
    fi
  fi
  if [[ -z "${lib_fl5}" ]]; then
    if [[ -f "${candidate}/libfl5-lib.so" || -f "${candidate}/libfl5-lib.a" ]]; then
      lib_fl5="${candidate}"
    fi
  fi
done

if [[ -z "${lib_xfoil}" || -z "${lib_fl5}" ]]; then
  echo "SKIP: XFoil/fl5-lib not found (set XFOIL_LIB_DIR and FL5_LIB_DIR to run)."
  exit 0
fi

cmake_args=(
  -DQT6_INCLUDE_DIR="${qt_include}"
  -DQT6_LIB_DIR="${qt_lib}"
  -DXFOIL_LIB_DIR="${lib_xfoil}"
  -DFL5_LIB_DIR="${lib_fl5}"
)

if [[ -n "${qt_cmake_dir}" ]]; then
  cmake_args+=(-DQt6_DIR="${qt_cmake_dir}")
fi

cmake -S "${root_dir}/API_examples/geometry_test" -B "${build_dir}" "${cmake_args[@]}"
cmake --build "${build_dir}"
OPENBLAS_NUM_THREADS=1 LD_LIBRARY_PATH="${lib_xfoil}:${lib_fl5}:${LD_LIBRARY_PATH:-}" "${build_dir}/GeometryTest"
