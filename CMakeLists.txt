cmake_minimum_required(VERSION 3.25)
project(ObjRender)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=znver4 -mavx512f -mavx512dq -mavx512vl -ffast-math") # -ffast-math -flto

find_package(raylib REQUIRED)

include_directories(include)

add_executable(objrender
    src/main.cpp
)

target_include_directories(objrender PUBLIC ${RAYLIB_CPP_INCLUDE_DIR} ${TINYOBJLOADER_INCLUDE_DIR})
target_link_libraries(objrender PUBLIC raylib)
