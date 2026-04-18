cmake_minimum_required(VERSION 3.16)
project(ConstraintTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Concurrent Widgets)

include(${CMAKE_CURRENT_LIST_DIR}/../flow5_test.cmake)

add_executable(ConstraintTest
    constraint_test.cpp
    ${OPTIFLOW_ROOT}/fl5-app/interfaces/optim/particle.cpp
    ${OPTIFLOW_ROOT}/fl5-app/interfaces/optim/psotask.cpp
    ${OPTIFLOW_ROOT}/fl5-app/interfaces/optim/psotaskfoil.cpp
)

target_link_libraries(ConstraintTest PRIVATE
    ${FLOW5_XFOIL_LIB}
    ${FLOW5_FL5_LIB}
    Qt6::Core Qt6::Concurrent Qt6::Widgets
)

include(GNUInstallDirs)
install(TARGETS ConstraintTest
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
