# Common setup shared by all Flow5 headless test CMakeLists in API_examples/.
#
# Callers must run project(...) and find_package(Qt6 ...) first, then include this
# file. It exposes:
#   OPTIFLOW_ROOT       - repo root (two levels up from the caller)
#   FLOW5_XFOIL_LIB     - absolute path to XFoil import library
#   FLOW5_FL5_LIB       - absolute path to fl5-lib import library
#
# Cache variables (settable on the cmake command line):
#   XFOIL_LIB_DIR       - hint for XFoil library search (qmake puts .lib/.so here)
#   FL5_LIB_DIR         - hint for fl5-lib library search
#   OCCT_INCLUDE_DIR    - OpenCASCADE include directory (only needed by 3D tests)

set(OPTIFLOW_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

set(XFOIL_LIB_DIR    "" CACHE PATH "XFoil library directory")
set(FL5_LIB_DIR      "" CACHE PATH "fl5-lib library directory")
set(OCCT_INCLUDE_DIR "" CACHE PATH "OpenCASCADE include directory (3D tests only)")

include_directories(
    ${OPTIFLOW_ROOT}/XFoil-lib
    ${OPTIFLOW_ROOT}/fl5-lib
    ${OPTIFLOW_ROOT}/fl5-lib/api
    ${OPTIFLOW_ROOT}/fl5-app
)

if(UNIX AND NOT APPLE)
    include_directories(
        /usr/local/include/XFoil
        /usr/local/include/fl5-lib
        /usr/local/include/fl5-lib/api
    )
endif()

if(OCCT_INCLUDE_DIR)
    include_directories(${OCCT_INCLUDE_DIR})
elseif(UNIX AND NOT APPLE)
    include_directories(/usr/include/opencascade /usr/local/include/opencascade)
endif()

# qmake emits XFoil1.lib / fl5-lib1.lib on Windows (VERSION suffix) and
# libXFoil.so / libfl5-lib.so on Linux. find_library handles both.
find_library(FLOW5_XFOIL_LIB
    NAMES XFoil XFoil1
    HINTS
        ${XFOIL_LIB_DIR}
        ${XFOIL_LIB_DIR}/release
        ${OPTIFLOW_ROOT}/XFoil-lib
        ${OPTIFLOW_ROOT}/XFoil-lib/release
        /usr/local/lib
        /usr/lib64
    REQUIRED
)

find_library(FLOW5_FL5_LIB
    NAMES fl5-lib fl5-lib1
    HINTS
        ${FL5_LIB_DIR}
        ${FL5_LIB_DIR}/release
        ${OPTIFLOW_ROOT}/fl5-lib
        ${OPTIFLOW_ROOT}/fl5-lib/release
        /usr/local/lib
        /usr/lib64
    REQUIRED
)

# XFoil is translated from Fortran and stack-allocates very large local arrays.
# Linux main-thread stack is 8 MB by default, Windows MSVC is only 1 MB, so tests
# that run XFoil on the main thread (xfoilrun) overflow instantly. Match Linux.
if(MSVC)
    add_link_options("/STACK:8388608")
endif()

# Windows MSVC does NOT propagate transitive shared-library dependencies the way
# ELF .so resolution does; fl5-lib.dll leaves OCCT kernel symbols (e.g.
# Standard_OutOfMemory::ctor) as external imports, so any test exe that includes
# fl5-lib/api headers which transitively pull OCCT must link TKernel.lib itself.
# On Linux the dynamic linker resolves these via libfl5-lib.so's DT_NEEDED entry.
function(flow5_test_link_occt target)
    if(NOT WIN32)
        return()
    endif()
    if(NOT OCCT_INCLUDE_DIR)
        message(FATAL_ERROR
            "flow5_test_link_occt(${target}) requires -DOCCT_INCLUDE_DIR=... "
            "(usually <OCCT_DIR>/include/opencascade)")
    endif()
    # Derive the sibling lib dir from the include dir: .../include/opencascade → .../lib
    get_filename_component(_occt_root "${OCCT_INCLUDE_DIR}" DIRECTORY)
    get_filename_component(_occt_root "${_occt_root}" DIRECTORY)
    find_library(FLOW5_OCCT_TKERNEL
        NAMES TKernel
        HINTS "${_occt_root}/lib"
        REQUIRED
    )
    target_link_libraries(${target} PRIVATE ${FLOW5_OCCT_TKERNEL})
endfunction()
