cmake_minimum_required(VERSION 3.16)
project(le_bounds_test)

find_package(Qt6 COMPONENTS Core REQUIRED)

add_executable(le_bounds_test le_bounds_test.cpp)

target_link_libraries(le_bounds_test PRIVATE 
    Qt6::Core
    fl5-lib
)

# Includes
target_include_directories(le_bounds_test PRIVATE
    ${CMAKE_SOURCE_DIR}/../../fl5-lib
    ${CMAKE_SOURCE_DIR}/../../fl5-lib/api
    ${CMAKE_SOURCE_DIR}/../../fl5-app
    ${CMAKE_SOURCE_DIR}/../../XFoil-lib
)

# Link directories
target_link_directories(le_bounds_test PRIVATE
    ${CMAKE_SOURCE_DIR}/../../fl5-lib
    ${CMAKE_SOURCE_DIR}/../../fl5-app
    ${CMAKE_SOURCE_DIR}/../../XFoil-lib
)
