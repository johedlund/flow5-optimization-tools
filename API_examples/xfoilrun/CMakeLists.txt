cmake_minimum_required(VERSION 3.16)

project(XFoilRun LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

set(OPTIFLOW_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

if (WIN32)
include_directories(D:/dev/flow5/XFoil-lib)
include_directories(D:/dev/flow5/fl5-lib)
include_directories(D:/dev/flow5/fl5-lib/api)
include_directories(C:/Qt/6.9.1/msvc2022_64/include)
include_directories(C:/Qt/6.9.1/msvc2022_64/include/QtCore)

link_directories(D:/dev/build/flow5/release/fl5-lib)
link_directories(D:/dev/build/flow5/release/XFoil-lib)
link_directories(C:/Qt/6.9.1/msvc2022_64/lib)
set(CMAKE_CXX_FLAGS /Zc:__cplusplus)
else ()
include_directories(${OPTIFLOW_ROOT}/XFoil-lib)
include_directories(${OPTIFLOW_ROOT}/fl5-lib)
include_directories(${OPTIFLOW_ROOT}/fl5-lib/api)
include_directories(/usr/local/include/XFoil/)
include_directories(/usr/local/include/fl5-lib/)
include_directories(/usr/local/include/fl5-lib/api/)

set(QT6_INCLUDE_DIR "" CACHE PATH "Qt6 include root")
set(QT6_LIB_DIR "" CACHE PATH "Qt6 library directory")
set(XFOIL_LIB_DIR "" CACHE PATH "XFoil library directory")
set(FL5_LIB_DIR "" CACHE PATH "fl5-lib library directory")

if (QT6_INCLUDE_DIR)
include_directories(${QT6_INCLUDE_DIR})
include_directories(${QT6_INCLUDE_DIR}/QtCore)
else ()
include_directories(/usr/include/qt6)
include_directories(/usr/include/qt6/QtCore)
include_directories(/usr/include/x86_64-linux-gnu/qt6)
include_directories(/usr/include/x86_64-linux-gnu/qt6/QtCore)
endif ()

link_directories(/usr/local/lib/)
link_directories(/usr/lib64/)
if (QT6_LIB_DIR)
link_directories(${QT6_LIB_DIR})
endif ()
if (XFOIL_LIB_DIR)
link_directories(${XFOIL_LIB_DIR})
endif ()
if (FL5_LIB_DIR)
link_directories(${FL5_LIB_DIR})
endif ()
endif (WIN32)

add_executable(XFoilRun xfoilrun.cpp)

target_link_libraries(XFoilRun PRIVATE XFoil fl5-lib Qt6::Core)

include(GNUInstallDirs)
install(TARGETS XFoilRun
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
