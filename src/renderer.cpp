#include "renderer.hpp"
#include "config.hpp"

#include <GL/glew.h>
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
    const int height = GetScreenHeight() - COMPACT_MENU_HEIGHT;

    graph_target = LoadRenderTexture(width, height);
    klotski_target = LoadRenderTexture(width, height);
    menu_target = LoadRenderTexture(width * 2, COMPACT_MENU_HEIGHT);
}

auto renderer::ensure_edge_buffers(const size_t mass_count, const size_t index_count) -> void
{
    if (edge_vao_id != 0 && mass_count <= edge_position_capacity &&
        index_count <= edge_index_capacity) {
        return;
    }

    if (edge_vao_id != 0) {
        rlUnloadVertexArray(edge_vao_id);
        rlUnloadVertexBuffer(edge_position_vbo_id);
        rlUnloadVertexBuffer(edge_index_ebo_id);
    }

    edge_position_capacity = std::max<size_t>(mass_count, 1024);
    edge_index_capacity = std::max<size_t>(index_count, 2048);

    edge_vao_id = rlLoadVertexArray();
    rlEnableVertexArray(edge_vao_id);

    edge_position_vbo_id = rlLoadVertexBuffer(
        nullptr, static_cast<int>(edge_position_capacity * sizeof(Vector3)), true);
    rlEnableVertexBuffer(edge_position_vbo_id);
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, sizeof(Vector3), 0);
    rlEnableVertexAttribute(0);
    rlDisableVertexBuffer();

    // The element buffer binding is part of the VAO state, so this has to happen while the VAO
    // is bound.
    edge_index_ebo_id = rlLoadVertexBufferElement(
        nullptr, static_cast<int>(edge_index_capacity * sizeof(u32)), true);

    rlDisableVertexArray();

    // Share the position buffer with the cube instancing VAO. The vertex shader adds the
    // per-instance position to the cube's local vertex position, so no matrices are needed.
    rlEnableVertexArray(cube_instance.vaoId);
    rlEnableVertexBuffer(edge_position_vbo_id);
    rlSetVertexAttribute(INSTANCE_POSITION_ATTR, 3, RL_FLOAT, false, sizeof(Vector3), 0);
    rlEnableVertexAttribute(INSTANCE_POSITION_ATTR);
    rlSetVertexAttributeDivisor(INSTANCE_POSITION_ATTR, 1);
    rlDisableVertexBuffer();
    rlDisableVertexArray();
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

    // Upload the per-mass positions and rebuild the static edge index list when the graph changes
    {
#ifdef TRACY
        ZoneNamedN(prepare_edge_buffers, "PrepareEdgeBuffers", true);
#endif

        const size_t link_count = state.get_link_count();
        const bool rebuild_indices = link_count != edge_last_link_count;

        if (rebuild_indices) {
            edge_indices.clear();
            edge_indices.reserve(link_count * 2);
            for (const auto& [from, to] : state.get_links()) {
                edge_indices.push_back(static_cast<u32>(from));
                edge_indices.push_back(static_cast<u32>(to));
            }
            edge_index_count = edge_indices.size();
        }

        ensure_edge_buffers(masses.size(), edge_index_count);

        rlUpdateVertexBuffer(edge_position_vbo_id, masses.data(),
                             static_cast<int>(masses.size() * sizeof(Vector3)), 0);

        if (rebuild_indices) {
            if (edge_index_count > 0) {
                rlUpdateVertexBufferElements(edge_index_ebo_id, edge_indices.data(),
                                             static_cast<int>(edge_indices.size() * sizeof(u32)),
                                             0);
            }
            edge_last_link_count = link_count;
        }
    }

    // Prepare connection drawing
    {
#ifdef TRACY
        ZoneNamedN(prepare_connections, "PrepareConnectionsDrawing", true);
#endif

        connections.clear();
        connections.reserve(state.get_target_count());
        if (input.connect_solutions) {
            for (const size_t& _state : state.get_winning_indices()) {
                const Vector3& current_mass = masses[state.get_current_index()];
                const Vector3& winning_mass = masses[_state];
                connections.emplace_back(current_mass, winning_mass);
            }
        }
    }

    // Prepare cube instancing
    {
#ifdef TRACY
        ZoneNamedN(prepare_masses, "PrepareMassInstancing", true);
#endif

        if (masses.size() < DRAW_VERTICES_LIMIT) {
            // Collisions
            // TODO: This would benefit greatly from a spatial data structure.
            //       Would it be worth to copy the octree from the physics thread?
            input.collision_mass = -1;
            if (input.mouse_in_graph_pane() && IsKeyDown(KEY_Q)) {
#ifdef TRACY
                ZoneNamedN(mass_collisions, "MassCollisions", true);
#endif
                const Ray ray = GetScreenToWorldRayEx(
                    GetMousePosition() - Vector2(GetScreenWidth() / 2.0f, COMPACT_MENU_HEIGHT),
                    camera.camera, graph_target.texture.width, graph_target.texture.height);
                // Ray collision hit info

                size_t mass = 0;
                for (const auto& [x, y, z] : masses) {
                    const RayCollision collision = GetRayCollisionBox(
                        ray,
                        BoundingBox{{x - VERTEX_SIZE / 2.0f, y - VERTEX_SIZE / 2.0f,
                                     z - VERTEX_SIZE / 2.0f},
                                    {x + VERTEX_SIZE / 2.0f, y + VERTEX_SIZE / 2.0f,
                                     z + VERTEX_SIZE / 2.0f}});
                    if (collision.hit) {
                        input.collision_mass = mass;
                        break;
                    }
                    ++mass;
                }
            }

            // Colors only depend on the displayed state and options, not on the positions. Rebuild
            // only when one of those actually changed (moves, selection, graph, display toggles).
            const bool rebuild_colors = !colors_valid || color_mass_count != masses.size() ||
                color_total_moves != state.get_total_moves() ||
                color_starting_index != state.get_starting_index() ||
                color_target_count != state.get_target_count() ||
                color_path_length != state.get_path_length() ||
                color_link_count != state.get_link_count() ||
                color_collision_mass != input.collision_mass ||
                color_mark_solutions != input.mark_solutions ||
                color_mark_path != input.mark_path ||
                color_by_distance_cached != input.color_by_distance;

            if (rebuild_colors) {
                colors.clear();
                colors.reserve(masses.size());

                // Find max distance to interpolate colors in the given [0, max] range
                int max_distance = 0;
                const std::vector<int>& distances = state.get_distances();
                for (const int distance : distances) {
                    if (distance > max_distance) {
                        max_distance = distance;
                    }
                }

                const auto lerp_color = [&](const Color from, const Color to, const int distance)
                {
                    const float weight = max_distance > 0
                        ? 1.0f - static_cast<float>(distance) / static_cast<float>(max_distance)
                        : 1.0f;

                    Color result;
                    result.r = static_cast<u8>((1 - weight) * from.r + weight * to.r);
                    result.g = static_cast<u8>((1 - weight) * from.g + weight * to.g);
                    result.b = static_cast<u8>((1 - weight) * from.b + weight * to.b);
                    result.a = static_cast<u8>((1 - weight) * from.a + weight * to.a);

                    return result;
                };

                size_t mass = 0;
                for ([[maybe_unused]] const auto& position : masses) {
                    // Normal vertex
                    Color c = VERTEX_COLOR;
                    if ((input.mark_solutions || input.mark_path) &&
                        state.get_winning_indices().contains(mass)) {
                        // Winning vertex
                        c = VERTEX_TARGET_COLOR;
                    } else if ((input.mark_solutions || input.mark_path) &&
                               state.get_path_indices().contains(mass)) {
                        // Path vertex
                        c = VERTEX_PATH_COLOR;
                    } else if (mass == state.get_starting_index()) {
                        // Starting vertex
                        c = VERTEX_START_COLOR;
                    } else if (state.get_visit_counts().at(mass) > 0) {
                        // Visited vertex
                        c = VERTEX_VISITED_COLOR;
                    } else if (input.color_by_distance && distances.size() == masses.size()) {
                        c = lerp_color(VERTEX_FARTHEST_COLOR, VERTEX_CLOSEST_COLOR,
                                       static_cast<int>(distances[mass]));
                    }
                    if (mass == input.collision_mass) {
                        c = RED;
                    }

                    // Current vertex is drawn as individual cube to increase its size
                    colors.emplace_back(c);
                    ++mass;
                }

                rlUpdateVertexBuffer(color_vbo_id, colors.data(), colors.size() * sizeof(Color), 0);

                colors_valid = true;
                color_mass_count = masses.size();
                color_total_moves = state.get_total_moves();
                color_starting_index = state.get_starting_index();
                color_target_count = state.get_target_count();
                color_path_length = state.get_path_length();
                color_link_count = state.get_link_count();
                color_collision_mass = input.collision_mass;
                color_mark_solutions = input.mark_solutions;
                color_mark_path = input.mark_path;
                color_by_distance_cached = input.color_by_distance;
            }
        }
    }

    BeginTextureMode(graph_target);
    ClearBackground(RAYWHITE);
    BeginMode3D(camera.camera);

    rlDrawRenderBatchActive();

    // Draw edges
    {
#ifdef TRACY
        ZoneNamedN(draw_springs, "DrawSprings", true);
#endif

        rlEnableShader(edge_shader.id);

        Matrix modelview = rlGetMatrixModelview();
        Matrix projection = rlGetMatrixProjection();
        Matrix mvp = MatrixMultiply(modelview, projection);
        rlSetUniformMatrix(edge_shader.locs[SHADER_LOC_MATRIX_MVP], mvp);

        const std::array<float, 4> edge_color = {EDGE_COLOR.r / 255.0f, EDGE_COLOR.g / 255.0f,
                                                 EDGE_COLOR.b / 255.0f, EDGE_COLOR.a / 255.0f};
        rlSetUniform(edge_color_loc, edge_color.data(), SHADER_UNIFORM_VEC4, 1);

        if (edge_index_count > 0) {
            glBindVertexArray(edge_vao_id);
            glDrawElements(GL_LINES, static_cast<GLsizei>(edge_index_count), GL_UNSIGNED_INT,
                           nullptr);
            glBindVertexArray(0);
        }

        rlDisableShader();

        // This draws triangles:
        // rlEnableVertexArray(edge_vao_id);
        // rlColor4ub(EDGE_COLOR.r, EDGE_COLOR.g, EDGE_COLOR.b, EDGE_COLOR.a);
        // rlDrawVertexArray(0, edge_vertices.size());
        // rlDisableVertexArray();

        // This is fucking slow:
        // rlBegin(RL_LINES);
        // for (const auto& [from, to] : state.get_links()) {
        //     if (masses.size() > from && masses.size() > to) {
        //         const auto& [ax, ay, az] = masses[from];
        //         const auto& [bx, by, bz] = masses[to];
        //         rlColor4ub(EDGE_COLOR.r, EDGE_COLOR.g, EDGE_COLOR.b, EDGE_COLOR.a);
        //         rlVertex3f(ax, ay, az);
        //         rlVertex3f(bx, by, bz);
        //     }
        // }
        // rlEnd();
    }

    // Draw masses (instanced)
    {
#ifdef TRACY
        ZoneNamedN(draw_masses, "DrawMasses", true);
#endif

        if (masses.size() < DRAW_VERTICES_LIMIT) {
            // Draw the cubes ourselves instead of using DrawMeshInstanced: raylib would upload a
            // 64-byte matrix per instance, while the shader only needs the 12-byte position that
            // is already in the shared position buffer.
            rlEnableShader(instancing_shader.id);

            Matrix modelview = rlGetMatrixModelview();
            Matrix projection = rlGetMatrixProjection();
            Matrix mvp = MatrixMultiply(modelview, projection);
            rlSetUniformMatrix(instancing_shader.locs[SHADER_LOC_MATRIX_MVP], mvp);

            glBindVertexArray(cube_instance.vaoId);
            glDrawElementsInstanced(GL_TRIANGLES, cube_instance.triangleCount * 3,
                                    GL_UNSIGNED_SHORT, nullptr,
                                    static_cast<GLsizei>(masses.size()));
            glBindVertexArray(0);

            rlDisableShader();
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
        DrawCube(current_mass, VERTEX_SIZE * 2, VERTEX_SIZE * 2, VERTEX_SIZE * 2,
                 VERTEX_CURRENT_COLOR);
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

auto renderer::draw_textures(const int fps, const int ups, const size_t mass_count,
                             const size_t spring_count) const -> void
{
    BeginDrawing();

    DrawTextureRec(menu_target.texture,
                   Rectangle(0, 0, menu_target.texture.width, -menu_target.texture.height),
                   Vector2(0, 0), WHITE);
    DrawTextureRec(klotski_target.texture,
                   Rectangle(0, 0, klotski_target.texture.width, -klotski_target.texture.height),
                   Vector2(0, COMPACT_MENU_HEIGHT), WHITE);
    DrawTextureRec(graph_target.texture,
                   Rectangle(0, 0, graph_target.texture.width, -graph_target.texture.height),
                   Vector2(GetScreenWidth() / 2.0f, COMPACT_MENU_HEIGHT), WHITE);

    // Draw borders
    DrawRectangleLinesEx(Rectangle(0, 0, GetScreenWidth(), COMPACT_MENU_HEIGHT), 1.0f, BLACK);
    DrawRectangleLinesEx(
        Rectangle(0, COMPACT_MENU_HEIGHT, GetScreenWidth() / 2.0f, GetScreenHeight() - COMPACT_MENU_HEIGHT), 1.0f,
        BLACK);
    DrawRectangleLinesEx(Rectangle(GetScreenWidth() / 2.0f, COMPACT_MENU_HEIGHT, GetScreenWidth() / 2.0f,
                                   GetScreenHeight() - COMPACT_MENU_HEIGHT),
                         1.0f, BLACK);

    gui.draw(fps, ups, mass_count, spring_count);

    EndDrawing();
}

auto renderer::render(const std::vector<Vector3>& masses, const int fps, const int ups,
                      const size_t mass_count, const size_t spring_count) -> void
{
    update_texture_sizes();

    draw_mass_springs(masses);
    draw_klotski();
    draw_menu();
    draw_textures(fps, ups, mass_count, spring_count);
}
