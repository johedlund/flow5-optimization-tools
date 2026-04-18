cmake_minimum_required(VERSION 3.16)
project(XFoilRun LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

include(${CMAKE_CURRENT_LIST_DIR}/../flow5_test.cmake)

add_executable(XFoilRun xfoilrun.cpp)

target_link_libraries(XFoilRun PRIVATE
    ${FLOW5_XFOIL_LIB}
    ${FLOW5_FL5_LIB}
    Qt6::Core
)

include(GNUInstallDirs)
install(TARGETS XFoilRun
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
