#include "renderer.hpp"
#include "config.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

auto renderer::update_texture_sizes() -> void
{
    if (!IsWindowResized()) {
        return;
    }

    UnloadRenderTexture(graph_target);
    UnloadRenderTexture(klotski_target);
    UnloadRenderTexture(menu_target);

    const int width = GetScreenWidth() / 2;
    const int height = GetScreenHeight() - MENU_HEIGHT;

    graph_target = LoadRenderTexture(width, height);
    klotski_target = LoadRenderTexture(width, height);
    menu_target = LoadRenderTexture(width * 2, MENU_HEIGHT);
}

auto renderer::draw_mass_springs(const std::vector<Vector3>& masses) -> void
{
    #ifdef TRACY
    ZoneScoped;
    #endif

    if (masses.size() != state.get_state_count()) {
        // Because the physics run in a different thread, it might need time to catch up
        return;
    }

    // Prepare connection batching
    {
        #ifdef TRACY
        ZoneNamedN(prepare_connections, "PrepareConnectionsBatching", true);
        #endif

        connections.clear();
        connections.reserve(state.get_target_count());
        if (input.connect_solutions) {
            for (const size_t& _state : state.get_winning_indices()) {
                const Vector3& current_mass = masses[state.get_current_index()];
                const Vector3& winning_mass = masses[_state];
                connections.emplace_back(current_mass, winning_mass);
                DrawLine3D(current_mass, winning_mass, Fade(TARGET_BLOCK_COLOR, 0.5));
            }
        }
    }

    // Prepare cube instancing
    {
        #ifdef TRACY
        ZoneNamedN(prepare_masses, "PrepareMassInstancing", true);
        #endif

        if (masses.size() < DRAW_VERTICES_LIMIT) {
            // Don't have to reserve, capacity is already set to DRAW_VERTICES_LIMIT in constructor
            transforms.clear();
            colors.clear();

            // Collisions
            // TODO: This would benefit greatly from a spatial data structure.
            //       Would it be worth to copy the octree from the physics thread?
            input.collision_mass = -1;
            if (input.mouse_in_graph_pane()) {
                const Ray ray = GetScreenToWorldRayEx(
                    GetMousePosition() - Vector2(GetScreenWidth() / 2.0f, MENU_HEIGHT),
                    camera.camera, graph_target.texture.width, graph_target.texture.height);
                RayCollision collision; // Ray collision hit info

                size_t mass = 0;
                for (const auto& [x, y, z] : masses) {
                    collision = GetRayCollisionBox(ray,
                                                   BoundingBox{
                                                       {
                                                           x - VERTEX_SIZE / 2.0f,
                                                           y - VERTEX_SIZE / 2.0f,
                                                           z - VERTEX_SIZE / 2.0f
                                                       },
                                                       {
                                                           x + VERTEX_SIZE / 2.0f,
                                                           y + VERTEX_SIZE / 2.0f,
                                                           z + VERTEX_SIZE / 2.0f
                                                       }
                                                   });
                    if (collision.hit) {
                        input.collision_mass = mass;
                        break;
                    }
                    ++mass;
                }
            }

            // Find max distance to interpolate colors in the given [0, max] range
            int max_distance = 0;
            for (const int distance : state.get_distances()) {
                if (distance > max_distance) {
                    max_distance = distance;
                }
            }

            const auto lerp_color = [&](const Color from, const Color to, const int distance)
            {
                const float weight = 1.0 - static_cast<float>(distance) / max_distance;

                Color result;
                result.r = static_cast<uint8_t>((1 - weight) * from.r + weight * to.r);
                result.g = static_cast<uint8_t>((1 - weight) * from.g + weight * to.g);
                result.b = static_cast<uint8_t>((1 - weight) * from.b + weight * to.b);
                result.a = static_cast<uint8_t>((1 - weight) * from.a + weight * to.a);

                return result;
            };

            const std::vector<int>& distances = state.get_distances();

            size_t mass = 0;
            for (const auto& [x, y, z] : masses) {
                transforms.emplace_back(MatrixTranslate(x, y, z));

                // Normal vertex
                Color c = VERTEX_COLOR;
                if ((input.mark_solutions || input.mark_path) && state.get_winning_indices().contains(mass)) {
                    // Winning vertex
                    c = VERTEX_TARGET_COLOR;
                } else if ((input.mark_solutions || input.mark_path) && state.get_path_indices().contains(mass)) {
                    // Path vertex
                    c = VERTEX_PATH_COLOR;
                } else if (mass == state.get_starting_index()) {
                    // Starting vertex
                    c = VERTEX_START_COLOR;
                } else if (state.get_visit_counts().at(mass) > 0) {
                    // Visited vertex
                    c = VERTEX_VISITED_COLOR;
                } else if (input.color_by_distance && distances.size() == masses.size()) {
                    c = lerp_color(VERTEX_FARTHEST_COLOR, VERTEX_CLOSEST_COLOR, static_cast<float>(distances[mass]));
                }
                if (mass == input.collision_mass) {
                    c = RED;
                }

                // Current vertex is drawn as individual cube to increase its size
                colors.emplace_back(c);
                ++mass;
            }
        }

        rlUpdateVertexBuffer(color_vbo_id, colors.data(), colors.size() * sizeof(Color), 0);
    }

    BeginTextureMode(graph_target);
    ClearBackground(RAYWHITE);
    BeginMode3D(camera.camera);

    // Draw springs (batched)
    {
        #ifdef TRACY
        ZoneNamedN(draw_springs, "DrawSprings", true);
        #endif

        rlBegin(RL_LINES);
        for (const auto& [from, to] : state.get_links()) {
            if (masses.size() > from && masses.size() > to) {
                const auto& [ax, ay, az] = masses[from];
                const auto& [bx, by, bz] = masses[to];
                rlColor4ub(EDGE_COLOR.r, EDGE_COLOR.g, EDGE_COLOR.b, EDGE_COLOR.a);
                rlVertex3f(ax, ay, az);
                rlVertex3f(bx, by, bz);
            }
        }
        rlEnd();
    }

    // Draw masses (instanced)
    {
        #ifdef TRACY
        ZoneNamedN(draw_masses, "DrawMasses", true);
        #endif

        if (masses.size() < DRAW_VERTICES_LIMIT) {
            // NOTE: I don't know if drawing all this inside a shader would make it
            //       much faster... The amount of data sent to the GPU would be
            //       reduced (just positions instead of matrices), but is this
            //       noticable for < 100000 cubes?
            DrawMeshInstanced(cube_instance, vertex_mat, transforms.data(), masses.size()); // NOLINT(*-narrowing-conversions)
        }
    }

    // Connect current to winning states (batched)
    const auto [r, g, b, a] = Fade(VERTEX_CURRENT_COLOR, 0.3);
    rlBegin(RL_LINES);
    for (const auto& [from, to] : connections) {
        const auto& [ax, ay, az] = from;
        const auto& [bx, by, bz] = to;
        rlColor4ub(r, g, b, a);
        rlVertex3f(ax, ay, az);
        rlVertex3f(bx, by, bz);
    }
    rlEnd();

    // Mark current state
    const size_t current_index = state.get_current_index();
    if (masses.size() > current_index) {
        const Vector3& current_mass = masses[current_index];
        DrawCube(current_mass, VERTEX_SIZE * 2, VERTEX_SIZE * 2, VERTEX_SIZE * 2, VERTEX_CURRENT_COLOR);
    }

    EndMode3D();
    EndTextureMode();
}

auto renderer::draw_klotski() const -> void
{
    #ifdef TRACY
    ZoneScoped;
    #endif

    BeginTextureMode(klotski_target);
    ClearBackground(RAYWHITE);

    gui.draw_puzzle_board();

    EndTextureMode();
}

auto renderer::draw_menu() const -> void
{
    #ifdef TRACY
    ZoneScoped;
    #endif

    BeginTextureMode(menu_target);
    ClearBackground(RAYWHITE);

    gui.draw_main_menu();

    EndTextureMode();
}

auto renderer::draw_textures(const int fps,
                             const int ups,
                             const size_t mass_count,
                             const size_t spring_count) const -> void
{
    BeginDrawing();

    DrawTextureRec(menu_target.texture,
                   Rectangle(0, 0, menu_target.texture.width, -menu_target.texture.height),
                   Vector2(0, 0),
                   WHITE);
    DrawTextureRec(klotski_target.texture,
                   Rectangle(0, 0, klotski_target.texture.width, -klotski_target.texture.height),
                   Vector2(0, MENU_HEIGHT),
                   WHITE);
    DrawTextureRec(graph_target.texture,
                   Rectangle(0, 0, graph_target.texture.width, -graph_target.texture.height),
                   Vector2(GetScreenWidth() / 2.0f, MENU_HEIGHT),
                   WHITE);

    // Draw borders
    DrawRectangleLinesEx(Rectangle(0, 0, GetScreenWidth(), MENU_HEIGHT), 1.0f, BLACK);
    DrawRectangleLinesEx(Rectangle(0, MENU_HEIGHT, GetScreenWidth() / 2.0f, GetScreenHeight() - MENU_HEIGHT),
                         1.0f,
                         BLACK);
    DrawRectangleLinesEx(Rectangle(GetScreenWidth() / 2.0f,
                                   MENU_HEIGHT,
                                   GetScreenWidth() / 2.0f,
                                   GetScreenHeight() - MENU_HEIGHT),
                         1.0f,
                         BLACK);

    gui.draw(fps, ups, mass_count, spring_count);

    EndDrawing();
}

auto renderer::render(const std::vector<Vector3>& masses,
                      const int fps,
                      const int ups,
                      const size_t mass_count,
                      const size_t spring_count) -> void
{
    update_texture_sizes();

    draw_mass_springs(masses);
    draw_klotski();
    draw_menu();
    draw_textures(fps, ups, mass_count, spring_count);
}