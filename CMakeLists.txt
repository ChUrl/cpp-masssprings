cmake_minimum_required(VERSION 3.25)
project(MassSprings)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

option(DISABLE_BACKWARD "Disable backward stacktrace printer" OFF)
option(DISABLE_TRACY "Disable the Tracy profiler client" ON)
option(DISABLE_TESTS "Disable building and running tests" ON)

# Headers + Sources
set(SOURCES
    src/backward.cpp
    src/graph_distances.cpp
    src/input_handler.cpp
    src/mass_spring_system.cpp
    src/octree.cpp
    src/orbit_camera.cpp
    src/renderer.cpp
    src/state_manager.cpp
    src/threaded_physics.cpp
    src/user_interface.cpp
    src/puzzle.cpp
)

# Libraries
find_package(raylib REQUIRED)
find_package(Boost REQUIRED)
set(LIBS raylib Boost::headers)
set(FLAGS "")

if(WIN32)
    list(APPEND LIBS opengl32 gdi32 winmm)
endif()

include(FetchContent)
if(NOT DISABLE_BACKWARD)
    find_package(Backward REQUIRED)

    list(APPEND LIBS Backward::Backward)
    list(APPEND FLAGS BACKWARD)
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
endif()

# Set this after fetching tracy to hide tracy's warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wno-unused-parameter -Wunreachable-code")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ggdb -Ofast -march=native")

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

# Testing sources
if(NOT DISABLE_TESTS AND NOT WIN32)
    enable_testing()

    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.13.0
    )
    FetchContent_MakeAvailable(Catch2)

    set(TEST_SOURCES
        test/bits.cpp
    )

    add_executable(tests ${TEST_SOURCES} ${SOURCES})
    target_include_directories(tests PRIVATE include)
    target_link_libraries(tests Catch2::Catch2WithMain raylib)

    include(Catch)
    catch_discover_tests(tests)
endif()

# LTO
#if(NOT WIN32)
include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if(supported)
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET masssprings PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported")
endif()
#endif()