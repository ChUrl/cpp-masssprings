cmake_minimum_required(VERSION 3.25)
project(MassSprings)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wno-unused-parameter -Wunreachable-code")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O2 -ggdb") # -fsanitize=address already fails on InitWindow(), -fsanitize=undefined, -fsanitize=leak
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast -march=native")

message("-- CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
message("-- CMAKE_C_FLAGS_DEBUG: ${CMAKE_C_FLAGS_DEBUG}")
message("-- CMAKE_C_FLAGS_RELEASE: ${CMAKE_C_FLAGS_RELEASE}")
message("-- CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message("-- CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message("-- CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")

# Libraries
find_package(raylib REQUIRED)
set(LIBS raylib)
set(FLAGS "")

if(NOT DEFINED DISABLE_BACKWARD)
    find_package(Backward REQUIRED)

    list(APPEND LIBS Backward::Backward)
    list(APPEND FLAGS BACKWARD)
endif()

if(NOT DEFINED DISABLE_TRACY)
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

    list(APPEND LIBS TracyClient)
    list(APPEND FLAGS TRACY)
endif()

# Headers + Sources
include_directories(include)
set(SOURCES
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
    src/gui.cpp
)

# Main target
add_executable(masssprings ${SOURCES})
target_include_directories(masssprings PRIVATE ${RAYLIB_CPP_INCLUDE_DIR})
target_link_libraries(masssprings PRIVATE ${LIBS})
target_compile_definitions(masssprings PRIVATE ${FLAGS})

# LTO
if (NOT WIN32)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT supported OUTPUT error)
    if(supported)
        message(STATUS "IPO / LTO enabled")
        set_property(TARGET masssprings PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        if(USE_TRACY)
            set_property(TARGET masssprings_tracy PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    else()
        message(STATUS "IPO / LTO not supported: <${error}>")
    endif()
endif()
