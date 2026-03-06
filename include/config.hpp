#ifndef CONFIG_HPP_
#define CONFIG_HPP_

#include <raylib.h>

// Calculate the octree parallel to the layout calculation.
// Layout uses the octree from last frame.
#define ASYNC_OCTREE

// Gets set by CMake
// #define THREADPOOL // Enable physics threadpool
// #define BACKWARD   // Enable pretty stack traces
// #define TRACY      // Enable tracy profiling support

#ifdef TRACY
#include <tracy/Tracy.hpp>
#endif

#if defined(_WIN32)
#define NOGDI  // All GDI defines and routines
#define NOUSER // All USER defines and routines
#endif
#define BS_THREAD_POOL_NATIVE_EXTENSIONS
// ReSharper disable once CppUnusedIncludeDirective
#include <BS_thread_pool.hpp>
#if defined(_WIN32) // raylib uses these names as function parameters
#undef near
#undef far
#endif

// Window
constexpr int INITIAL_WIDTH = 600;
constexpr int INITIAL_HEIGHT = 600;
constexpr int MENU_HEIGHT = 300;
constexpr int POPUP_WIDTH = 450;
constexpr int POPUP_HEIGHT = 150;

// Menu
constexpr int MENU_PAD = 5;
constexpr int BUTTON_PAD = 12;
constexpr int MENU_ROWS = 7;
constexpr int MENU_COLS = 3;
constexpr const char* FONT = "fonts/SpaceMono.ttf";
constexpr int FONT_SIZE = 26;

// Camera Controls
constexpr float CAMERA_FOV = 90.0;
constexpr float FOV_SPEED = 1.0;
constexpr float FOV_MULTIPLIER = 4.0;
constexpr float MIN_FOV = 10.0;
constexpr float MAX_PERSP_FOV = 120.0;
constexpr float MAX_ORTHO_FOV = 540.0;
constexpr float CAMERA_DISTANCE = 150.0;
constexpr float MIN_CAMERA_DISTANCE = 2.0;
constexpr float MAX_CAMERA_DISTANCE = 2000.0;
constexpr float ZOOM_SPEED = 2.5;
constexpr float ZOOM_MULTIPLIER = 4.0;
constexpr float PAN_SPEED = 2.0;
constexpr float PAN_MULTIPLIER = 10.0;
constexpr float ROT_SPEED = 1.0;
constexpr float CAMERA_SMOOTH_SPEED = 15.0;

// Physics Engine
constexpr float TARGET_UPS = 90;             // How often to update physics
constexpr float TIMESTEP = 1.0 / TARGET_UPS; // Update interval in seconds
constexpr float SIM_SPEED = 4.0;             // How large each update should be
constexpr float MASS = 1.0;                  // Mass spring system
constexpr float SPRING_K = 4.0;              // Mass spring system
constexpr float DAMPENING_K = 1.0;           // Mass spring system
constexpr float REST_LENGTH = 3.0;           // Mass spring system
constexpr float VERLET_DAMPENING = 0.1;      // [0, 1]
constexpr float BH_FORCE = 2.5;              // Barnes-Hut [1.0, 3.0]
constexpr float THETA = 0.8;                 // Barnes-Hut [0.5, 1.0]
constexpr float SOFTENING = 0.05;            // Barnes-Hut [0.01, 1.0]

// Graph Drawing
static const Color EDGE_COLOR = Fade(BLUE, 0.3);
constexpr int DRAW_EDGES_LIMIT = 5'000'000;
constexpr float VERTEX_SIZE = 0.75;
constexpr int DRAW_VERTICES_LIMIT = 1'000'000;
static const Color VERTEX_COLOR = Fade(BLUE, 0.8);
constexpr Color VERTEX_VISITED_COLOR = ORANGE;
constexpr Color VERTEX_START_COLOR = ORANGE;
constexpr Color VERTEX_CURRENT_COLOR = ORANGE;
constexpr Color VERTEX_PATH_COLOR = GREEN;
constexpr Color VERTEX_TARGET_COLOR = GREEN;
static const Color VERTEX_CLOSEST_COLOR = Fade(PINK, 1.0);
static const Color VERTEX_FARTHEST_COLOR = Fade(DARKBLUE, 0.8);

// Klotski Drawing
constexpr int BOARD_PADDING = 10;
constexpr Color BOARD_COLOR_WON = DARKGREEN;
constexpr Color BOARD_COLOR_RESTRICTED = GRAY;
constexpr Color BOARD_COLOR_FREE = RAYWHITE;
constexpr Color BLOCK_COLOR = DARKBLUE;
constexpr Color TARGET_BLOCK_COLOR = RED;
constexpr Color WALL_COLOR = BLACK;

// Threadpool
static constexpr int SMALL_TASK_BLOCK_SIZE = 256; // Weirdly larger blocks decrease performance...
static constexpr int LARGE_TASK_BLOCK_SIZE = 256;

#endif