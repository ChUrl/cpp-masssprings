cmake_minimum_required(VERSION 3.25)
project(MassSprings)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

find_package(raylib REQUIRED)
find_package(OpenMP REQUIRED)

# TODO: Need to enable/disable this based on a variable for nix build
if(USE_TRACY)
include(FetchContent)
FetchContent_Declare(tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG v0.11.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(tracy)
option(TRACY_ENABLE "" ON)
option(TRACY_ON_DEMAND "" ON)
endif()

include_directories(include)

add_executable(masssprings
    src/main.cpp
    src/camera.cpp
    src/renderer.cpp
    src/octree.cpp
    src/physics.cpp
    src/puzzle.cpp
    src/state.cpp
    src/input.cpp
)

target_include_directories(masssprings PUBLIC ${RAYLIB_CPP_INCLUDE_DIR})
if(USE_TRACY)
target_link_libraries(masssprings PUBLIC raylib OpenMP::OpenMP_CXX TracyClient)
else()
target_link_libraries(masssprings PUBLIC raylib OpenMP::OpenMP_CXX)
endif()
