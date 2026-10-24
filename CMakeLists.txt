cmake_minimum_required(VERSION 3.28)
project(MassSprings)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Disable boost warning because our cmake/boost are recent enough
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

option(DISABLE_THREADPOOL "Disable additional physics threads" OFF)
option(DISABLE_BACKWARD "Disable backward stacktrace printer" OFF)
option(DISABLE_TRACY "Disable the Tracy profiler client" OFF)
option(DISABLE_TESTS "Disable building tests" OFF)
option(DISABLE_BENCH "Disable building benchmarks" OFF)

# Headers + Sources (excluding main.cpp)
set(SOURCES
    src/backward.cpp
    src/bits.cpp
    src/cpu_layout_engine.cpp
    src/cpu_spring_system.cpp
    src/default_settings.cpp
    src/eight_puzzle_codec.cpp
    src/eight_puzzle_model.cpp
    src/eight_puzzle_presenter.cpp
    src/gpu_context.cpp
    src/gpu_octree.cpp
    src/gpu_physics.cpp
    src/graph_distances.cpp
    src/hanoi_codec.cpp
    src/hanoi_model.cpp
    src/hanoi_presenter.cpp
    src/input_handler.cpp
    src/lights_out_codec.cpp
    src/lights_out_model.cpp
    src/lights_out_presenter.cpp
    src/load_save.cpp
    src/memory_tracker.cpp
    src/octree.cpp
    src/orbit_camera.cpp
    src/placement_space.cpp
    src/puzzle.cpp
    src/puzzle_context.cpp
    src/puzzle_presenter.cpp
    src/puzzle_registry.cpp
    src/puzzle_space.cpp
    src/puzzle_space_io.cpp
    src/renderer.cpp
    src/search_controller.cpp
    src/settings.cpp
    src/sliding_block_codec.cpp
    src/sliding_block_model.cpp
    src/sliding_block_presenter.cpp
    src/state_graph.cpp
    src/state_manager.cpp
    src/sudoku4_codec.cpp
    src/sudoku4_model.cpp
    src/sudoku4_presenter.cpp
    src/user_interface.cpp
    src/wall_layout.cpp
)

# Libraries
include(FetchContent)
find_package(raylib REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
# Map Debug to the Release imported config so the real libGLEW.so is used.
if(TARGET GLEW::GLEW)
    set_target_properties(GLEW::GLEW PROPERTIES MAP_IMPORTED_CONFIG_DEBUG Release)
endif()
find_package(libmorton REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)
set(LIBS raylib GLEW::GLEW glfw Boost::headers Boost::program_options)
set(FLAGS "")

if(WIN32)
    list(APPEND LIBS opengl32 gdi32 winmm)
endif()

if(NOT DISABLE_THREADPOOL)
    list(APPEND FLAGS THREADPOOL)
endif()

if(NOT DISABLE_BACKWARD)
    find_package(Backward REQUIRED)

    list(APPEND LIBS Backward::Backward)
    list(APPEND FLAGS BACKWARD)

    message("-- BACKWARD: Enabled")
endif()

if(NOT DISABLE_TRACY)
    FetchContent_Declare(tracy
                         GIT_REPOSITORY https://github.com/wolfpld/tracy.git
                         GIT_TAG v0.13.1
                         GIT_SHALLOW TRUE
                         GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(tracy)
    option(TRACY_ENABLE "" ON)
    option(TRACY_ON_DEMAND "" ON)

    list(APPEND LIBS TracyClient)
    list(APPEND FLAGS TRACY)

    message("-- TRACY: Enabled")
endif()

# Set this after fetching tracy to hide tracy's warnings.
# We set -Wno-alloc-size-larger-than because it prevents BS::thread_pool from building with current gcc
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wno-unused-parameter -Wunreachable-code -Wno-alloc-size-larger-than")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb -O0")

# Target ISA. Local builds and the (host-specific) Linux package tune for this machine,
# shipped artifacts (the Windows cross build) use a fixed baseline. Do not rely on the
# toolchain's -march=native being filtered by a wrapper: pass MARCH explicitly.
if(CMAKE_CROSSCOMPILING)
    set(_march_default "x86-64-v3")
else()
    set(_march_default "native")
endif()
set(MARCH "${_march_default}" CACHE STRING "Value for -march (native, x86-64-v3, znver4, ...)")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ggdb -O3 -ffast-math -march=${MARCH}")

message("-- MARCH: ${MARCH}")
message("-- CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
message("-- CMAKE_C_FLAGS_DEBUG: ${CMAKE_C_FLAGS_DEBUG}")
message("-- CMAKE_C_FLAGS_RELEASE: ${CMAKE_C_FLAGS_RELEASE}")
message("-- CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message("-- CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message("-- CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")

# Main target
add_executable(masssprings src/main.cpp src/cli.cpp ${SOURCES})
target_include_directories(masssprings PRIVATE include)
target_link_libraries(masssprings PRIVATE ${LIBS})
target_compile_definitions(masssprings PRIVATE ${FLAGS})

# Testing
if(NOT DISABLE_TESTS AND NOT WIN32)
    enable_testing()

    FetchContent_Declare(Catch2
                         GIT_REPOSITORY https://github.com/catchorg/Catch2.git
                         GIT_TAG v3.13.0
    )
    FetchContent_MakeAvailable(Catch2)

    set(TEST_SOURCES
        test/bits.cpp
        test/bitmap.cpp
        test/bitmap_find_first_empty.cpp
        test/eight_puzzle.cpp
        test/graph_distances.cpp
        test/hanoi.cpp
        test/lights_out.cpp
        test/memory_tracker.cpp
        test/octree_parallel.cpp
        test/placement_space.cpp
        test/puzzle_canvas.cpp
        test/puzzle_context.cpp
        test/puzzle_space.cpp
        test/puzzle_space_integration.cpp
        test/search_controller.cpp
        test/state_graph.cpp
        test/state_space.cpp
        test/sudoku4.cpp
        # test/puzzle.cpp
    )

    add_executable(tests ${TEST_SOURCES} ${SOURCES})
    target_include_directories(tests PRIVATE include)
    target_link_libraries(tests Catch2::Catch2WithMain raylib GLEW::GLEW glfw)

    include(Catch)
    catch_discover_tests(tests)

    message("-- TESTS: Enabled")
endif()

# Benchmarking
if(NOT DISABLE_BENCH AND NOT WIN32)
    find_package(benchmark REQUIRED)

    set(BENCH_SOURCES
        benchmark/state_space.cpp
        benchmark/octree.cpp
    )

    add_executable(benchmarks ${BENCH_SOURCES} ${SOURCES})
    target_include_directories(benchmarks PRIVATE include)
    target_link_libraries(benchmarks benchmark raylib GLEW::GLEW glfw)

    message("-- BENCHMARKS: Enabled")
endif()

# LTO
include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if(supported)
    message("-- IPO/LTO: Enabled")
    set_property(TARGET masssprings PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message("-- IPO/LTO: Disabled")
endif()
