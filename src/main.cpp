#define VERLET_UPDATE

#include <iostream>
#include <raylib.h>
#include <raymath.h>

#include "config.hpp"
#include "klotski.hpp"
#include "mass_springs.hpp"
#include "renderer.hpp"

auto main(int argc, char *argv[]) -> int {
  // if (argc < 2) {
  //   std::cout << "Missing .klotski file." << std::endl;
  //   return 1;
  // }

  SetTraceLogLevel(LOG_ERROR);

  // SetTargetFPS(60);
  SetConfigFlags(FLAG_VSYNC_HINT);
  SetConfigFlags(FLAG_MSAA_4X_HINT);

  InitWindow(WIDTH * 2, HEIGHT, "MassSprings");

  MassSpringSystem mass_springs;
  mass_springs.AddMass(1.0, Vector3(-0.5, 0.5, 0.0), true);
  mass_springs.AddMass(1.0, Vector3(0.5, 0.5, 0.0), false);
  mass_springs.AddMass(1.0, Vector3(0.5, 0.0, 0.0), false);
  mass_springs.AddSpring(0, 1, DEFAULT_SPRING_CONSTANT,
                         DEFAULT_DAMPENING_CONSTANT, DEFAULT_REST_LENGTH);
  mass_springs.AddSpring(1, 2, DEFAULT_SPRING_CONSTANT,
                         DEFAULT_DAMPENING_CONSTANT, DEFAULT_REST_LENGTH);

  State s = State(4, 5);
  Block a = Block(0, 0, 2, 1, false);
  Block b = Block(0, 1, 1, 3, true);
  Block c = Block(0, 2, "45");
  Block d = Block(0, 3, "de");

  s.AddBlock(a);
  s.AddBlock(b);
  for (Block block : s) {
    std::cout << "Block (" << block.x << ", " << block.y << ")" << std::endl;
  }

  Renderer renderer(WIDTH, HEIGHT);
  Edge2Set edges;
  edges.reserve(mass_springs.springs.size());
  Vertex2Set vertices;
  vertices.reserve(mass_springs.masses.size());

  float frametime;
  float abstime = 0.0;
  while (!WindowShouldClose()) {

    frametime = GetFrameTime();
    mass_springs.ClearForces();
    mass_springs.CalculateSpringForces();
#ifdef VERLET_UPDATE
    mass_springs.VerletUpdate(frametime * SIM_SPEED);
#else
    mass_springs.EulerUpdate(frametime * SIM_SPEED);
#endif

    renderer.Transform(edges, vertices, mass_springs, abstime * ROTATION_SPEED,
                       CAMERA_DISTANCE);
    renderer.DrawMassSprings(edges, vertices);
    renderer.DrawKlotski(s); // TODO: Don't need to render each frame
    renderer.DrawTextures();

    abstime += frametime;
  }

  CloseWindow();

  return 0;
}
