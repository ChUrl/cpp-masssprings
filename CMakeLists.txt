cmake_minimum_required(VERSION 3.25)
project(MassSprings)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(raylib REQUIRED)

include_directories(include)

add_executable(masssprings
    src/main.cpp
    src/renderer.cpp
    src/mass_springs.cpp
)

target_include_directories(masssprings PUBLIC ${RAYLIB_CPP_INCLUDE_DIR})
target_link_libraries(masssprings PUBLIC raylib)
