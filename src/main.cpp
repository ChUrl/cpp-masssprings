#include "mass_springs.hpp"

#define VERLET_UPDATE

#include <iostream>
#include <raylib.h>
#include <raymath.h>

#include "config.hpp"
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

  InitWindow(WIDTH, HEIGHT, "MassSprings");

  Edge3Set edges;
  Edge2Set viewport;
  viewport.reserve(edges.size());

  MassSpringSystem mass_springs;
  mass_springs.AddMass(1.0, Vector3(-0.5, 0.5, 0.0), true);
  mass_springs.AddMass(1.0, Vector3(0.5, 0.5, 0.0), false);
  mass_springs.AddMass(1.0, Vector3(0.5, 0.0, 0.0), false);
  mass_springs.AddSpring(0, 1, DEFAULT_SPRING_CONSTANT,
                         DEFAULT_DAMPENING_CONSTANT, DEFAULT_REST_LENGTH);
  mass_springs.AddSpring(1, 2, DEFAULT_SPRING_CONSTANT,
                         DEFAULT_DAMPENING_CONSTANT, DEFAULT_REST_LENGTH);

  Renderer renderer(WIDTH, HEIGHT);

  float frametime;
  float abstime = 0.0;
  while (!WindowShouldClose()) {

    frametime = GetFrameTime();
    mass_springs.ClearForces();
    mass_springs.CalculateSpringForces();
#ifdef VERLET_UPDATE
    mass_springs.VerletUpdate(frametime * SIM_SPEED);
#else
    mass_springs.IntegrateVelocities(frametime * SIM_SPEED);
    mass_springs.IntegratePositions(frametime * SIM_SPEED);
#endif

    // std::cout << "Calculating Spring Forces: A: (" << massA.force.x << ", "
    //           << massA.force.y << ", " << massA.force.z << ") B: ("
    //           << massB.force.x << ", " << massB.force.y << ", " <<
    //           massB.force.z
    //           << ")" << std::endl;
    // std::cout << "Calculating Velocities: A: (" << massA.velocity.x << ", "
    //           << massA.velocity.y << ", " << massA.velocity.z << ") B: ("
    //           << massB.velocity.x << ", " << massB.velocity.y << ", "
    //           << massB.velocity.z << ")" << std::endl;
    // std::cout << "Calculating Positions: A: (" << massA.position.x << ", "
    //           << massA.position.y << ", " << massA.position.z << ") B: ("
    //           << massB.position.x << ", " << massB.position.y << ", "
    //           << massB.position.z << ")" << std::endl;

    renderer.Transform(viewport, mass_springs, 0.0, CAMERA_DISTANCE);
    renderer.Draw(viewport);

    abstime += frametime * SIM_SPEED;
  }

  CloseWindow();

  return 0;
}
