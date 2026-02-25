cmake_minimum_required(VERSION 3.25)
project(MassSprings)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(raylib REQUIRED)
find_package(Backward REQUIRED)

# Need to enable/disable this based on a variable for nix build
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

    # Enable tracy macros in the application
    add_compile_definitions(TRACY)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wno-unused-parameter -Wunreachable-code")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og -ggdb -fsanitize=undefined") # -fsanitize=address already fails on InitWindow()
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")

message("-- CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
message("-- CMAKE_C_FLAGS_DEBUG: ${CMAKE_C_FLAGS_DEBUG}")
message("-- CMAKE_C_FLAGS_RELEASE: ${CMAKE_C_FLAGS_RELEASE}")
message("-- CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message("-- CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message("-- CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")

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
    src/tracy.cpp
    src/backward.cpp
    src/distance.cpp
)

target_include_directories(masssprings PUBLIC ${RAYLIB_CPP_INCLUDE_DIR})

if(USE_TRACY)
    target_link_libraries(masssprings PUBLIC raylib Backward::Backward TracyClient)
else()
    target_link_libraries(masssprings PUBLIC raylib Backward::Backward)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if(supported)
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET masssprings PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()
