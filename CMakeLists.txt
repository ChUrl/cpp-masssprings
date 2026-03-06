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
    src/graph_distances.cpp
    src/input_handler.cpp
    src/load_save.cpp
    src/octree.cpp
    src/orbit_camera.cpp
    src/puzzle.cpp
    src/renderer.cpp
    src/state_manager.cpp
    src/user_interface.cpp
)

# Libraries
include(FetchContent)
find_package(raylib REQUIRED)
find_package(libmorton REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)
set(LIBS raylib Boost::headers Boost::program_options)
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
                         GIT_TAG v0.11.1
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
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ggdb -O3 -ffast-math -march=native")

message("-- CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
message("-- CMAKE_C_FLAGS_DEBUG: ${CMAKE_C_FLAGS_DEBUG}")
message("-- CMAKE_C_FLAGS_RELEASE: ${CMAKE_C_FLAGS_RELEASE}")
message("-- CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message("-- CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message("-- CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")

# Main target
add_executable(masssprings src/main.cpp ${SOURCES})
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
        # test/puzzle.cpp
    )

    add_executable(tests ${TEST_SOURCES} ${SOURCES})
    target_include_directories(tests PRIVATE include)
    target_link_libraries(tests Catch2::Catch2WithMain raylib)

    include(Catch)
    catch_discover_tests(tests)

    message("-- TESTS: Enabled")
endif()

# Benchmarking
if(NOT DISABLE_BENCH AND NOT WIN32)
    find_package(benchmark REQUIRED)

    set(BENCH_SOURCES
        benchmark/state_space.cpp
    )

    add_executable(benchmarks ${BENCH_SOURCES} ${SOURCES})
    target_include_directories(benchmarks PRIVATE include)
    target_link_libraries(benchmarks benchmark raylib)

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