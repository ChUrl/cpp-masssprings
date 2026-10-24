#include "renderer.hpp"
#include "config.hpp"
#include "settings.hpp"

#include <GL/glew.h>
#include <cmath>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <utility>

auto renderer::update_texture_sizes() -> void
{
    if (!IsWindowResized()) {
        return;
    }

    UnloadRenderTexture(graph_target);
    UnloadRenderTexture(board_target);
    UnloadRenderTexture(menu_target);

    graph_target = LoadRenderTexture(right_pane_width(), content_height());
    board_target = LoadRenderTexture(left_pane_width(), content_height());
    menu_target = LoadRenderTexture(GetScreenWidth(), COMPACT_MENU_HEIGHT);
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

    // Same for the billboard quad VAO.
    rlEnableVertexArray(quad_instance.vaoId);
    rlEnableVertexBuffer(edge_position_vbo_id);
    rlSetVertexAttribute(INSTANCE_POSITION_ATTR, 3, RL_FLOAT, false, sizeof(Vector3), 0);
    rlEnableVertexAttribute(INSTANCE_POSITION_ATTR);
    rlSetVertexAttributeDivisor(INSTANCE_POSITION_ATTR, 1);
    rlDisableVertexBuffer();
    rlDisableVertexArray();
}

auto renderer::ensure_color_buffer(const size_t count) -> void
{
    if (count <= color_vbo_capacity) {
        return;
    }

    if (color_vbo_id != 0) {
        rlUnloadVertexBuffer(color_vbo_id);
    }
    color_vbo_capacity = count;
    color_vbo_id = rlLoadVertexBuffer(nullptr, static_cast<int>(count * sizeof(Color)), true);

    const auto bind_color = [&](const unsigned int vao)
    {
        rlEnableVertexArray(vao);
        rlEnableVertexBuffer(color_vbo_id);
        rlSetVertexAttribute(INSTANCE_COLOR_ATTR, 4, RL_UNSIGNED_BYTE, true, 0, 0);
        rlEnableVertexAttribute(INSTANCE_COLOR_ATTR);
        rlSetVertexAttributeDivisor(INSTANCE_COLOR_ATTR, 1);
        rlDisableVertexBuffer();
        rlDisableVertexArray();
    };
    bind_color(cube_instance.vaoId);
    bind_color(quad_instance.vaoId);
}

auto renderer::ensure_size_buffer(const size_t count) -> void
{
    if (count <= size_vbo_capacity) {
        return;
    }

    if (size_vbo_id != 0) {
        rlUnloadVertexBuffer(size_vbo_id);
    }
    size_vbo_capacity = count;
    size_vbo_id = rlLoadVertexBuffer(nullptr, static_cast<int>(count * sizeof(float)), true);

    const auto bind_size = [&](const unsigned int vao)
    {
        rlEnableVertexArray(vao);
        rlEnableVertexBuffer(size_vbo_id);
        rlSetVertexAttribute(INSTANCE_SIZE_ATTR, 1, RL_FLOAT, false, 0, 0);
        rlEnableVertexAttribute(INSTANCE_SIZE_ATTR);
        rlSetVertexAttributeDivisor(INSTANCE_SIZE_ATTR, 1);
        rlDisableVertexBuffer();
        rlDisableVertexArray();
    };
    bind_size(cube_instance.vaoId);
    bind_size(quad_instance.vaoId);
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

        const simulation_settings cfg = get_settings();
        const int stride = std::max(1, cfg.edge_stride);
        const size_t link_count = state.get_link_count();
        const size_t generation = state.generation();
        const bool rebuild_indices = link_count != edge_last_link_count ||
            stride != edge_last_stride || generation != edge_last_generation;

        if (rebuild_indices) {
            edge_indices.clear();
            edge_indices.reserve((link_count / static_cast<size_t>(stride) + 1) * 2);
            size_t link_index = 0;
            for (const auto& [from, to] : state.get_links()) {
                if (link_index++ % static_cast<size_t>(stride) != 0) {
                    continue;
                }
                edge_indices.push_back(static_cast<u32>(from));
                edge_indices.push_back(static_cast<u32>(to));
            }
            edge_index_count = edge_indices.size();
            edge_last_stride = stride;
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
            edge_last_generation = generation;
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

        const simulation_settings cfg = get_settings();
        if (cfg.draw_nodes) {
            ensure_color_buffer(masses.size());
            const float half_vertex = cfg.vertex_size / 2.0f;

            // Collisions
            // TODO: This would benefit greatly from a spatial data structure.
            //       Would it be worth to copy the octree from the physics thread?
            input.collision_mass = -1;
            if (input.mouse_in_graph_pane() && IsKeyDown(KEY_Q)) {
#ifdef TRACY
                ZoneNamedN(mass_collisions, "MassCollisions", true);
#endif
                const Ray ray = GetScreenToWorldRayEx(
                    GetMousePosition() -
                        Vector2(static_cast<float>(right_pane_x()), COMPACT_MENU_HEIGHT),
                    camera.camera, graph_target.texture.width, graph_target.texture.height);
                // Ray collision hit info

                size_t mass = 0;
                for (const auto& [x, y, z] : masses) {
                    const RayCollision collision = GetRayCollisionBox(
                        ray,
                        BoundingBox{{x - half_vertex, y - half_vertex, z - half_vertex},
                                    {x + half_vertex, y + half_vertex, z + half_vertex}});
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
                color_by_distance_cached != input.color_by_distance ||
                color_generation != state.generation();

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
                color_generation = state.generation();
            }

            const auto size_changed = [](const float a, const float b)
            { return std::fabs(a - b) > 1e-6f; };
            const bool rebuild_sizes = rebuild_colors || !node_sizes_valid ||
                size_mass_count != masses.size() ||
                size_changed(size_vertex_cached, cfg.vertex_size) ||
                size_changed(size_solution_cached, cfg.solution_node_size) ||
                size_changed(size_visited_cached, cfg.visited_node_size);
            if (rebuild_sizes) {
                ensure_size_buffer(masses.size());
                node_sizes.resize(masses.size());

                const auto same_color = [](const Color a, const Color b)
                { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; };

                for (size_t i = 0; i < node_sizes.size(); ++i) {
                    const Color c = colors[i];
                    if (same_color(c, VERTEX_TARGET_COLOR) || same_color(c, VERTEX_PATH_COLOR)) {
                        node_sizes[i] = cfg.solution_node_size;
                    } else if (same_color(c, VERTEX_VISITED_COLOR) ||
                               same_color(c, VERTEX_START_COLOR)) {
                        node_sizes[i] = cfg.visited_node_size;
                    } else {
                        node_sizes[i] = cfg.vertex_size;
                    }
                }

                if (!node_sizes.empty()) {
                    rlUpdateVertexBuffer(size_vbo_id, node_sizes.data(),
                                         node_sizes.size() * sizeof(float), 0);
                }
                node_sizes_valid = true;
                size_mass_count = masses.size();
                size_vertex_cached = cfg.vertex_size;
                size_solution_cached = cfg.solution_node_size;
                size_visited_cached = cfg.visited_node_size;
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

        const simulation_settings cfg = get_settings();
        if (cfg.draw_edges && edge_index_count > 0) {
            glBindVertexArray(edge_vao_id);
            glDrawElements(GL_LINES, static_cast<GLsizei>(edge_index_count), GL_UNSIGNED_INT,
                           nullptr);
            glBindVertexArray(0);
        }

        rlDisableShader();
    }

    // Draw masses (instanced)
    {
#ifdef TRACY
        ZoneNamedN(draw_masses, "DrawMasses", true);
#endif

        const simulation_settings cfg = get_settings();
        if (cfg.draw_nodes) {
            Matrix modelview = rlGetMatrixModelview();
            Matrix projection = rlGetMatrixProjection();
            Matrix mvp = MatrixMultiply(modelview, projection);

            if (cfg.billboard_nodes) {
                // Camera-facing quad; the fragment shader discards the corners to make it round.
                rlEnableShader(billboard_shader.id);
                rlSetUniformMatrix(billboard_shader.locs[SHADER_LOC_MATRIX_MVP], mvp);

                const Vector3 forward =
                    Vector3Normalize(camera.camera.target - camera.camera.position);
                const Vector3 right =
                    Vector3Normalize(Vector3CrossProduct(forward, camera.camera.up));
                const Vector3 up = Vector3CrossProduct(right, forward);
                const std::array<float, 3> right_values = {right.x, right.y, right.z};
                const std::array<float, 3> up_values = {up.x, up.y, up.z};
                rlSetUniform(billboard_right_loc, right_values.data(), SHADER_UNIFORM_VEC3, 1);
                rlSetUniform(billboard_up_loc, up_values.data(), SHADER_UNIFORM_VEC3, 1);

                rlDisableBackfaceCulling();
                glBindVertexArray(quad_instance.vaoId);
                glDrawElementsInstanced(GL_TRIANGLES, quad_instance.triangleCount * 3,
                                        GL_UNSIGNED_SHORT, nullptr,
                                        static_cast<GLsizei>(masses.size()));
                glBindVertexArray(0);
                rlEnableBackfaceCulling();

                rlDisableShader();
            } else {
                // Draw the cubes ourselves instead of using DrawMeshInstanced: raylib would upload
                // a 64-byte matrix per instance, while the shader only needs the 12-byte position
                // and the per-node size already in the instance buffers.
                rlEnableShader(instancing_shader.id);
                rlSetUniformMatrix(instancing_shader.locs[SHADER_LOC_MATRIX_MVP], mvp);

                glBindVertexArray(cube_instance.vaoId);
                glDrawElementsInstanced(GL_TRIANGLES, cube_instance.triangleCount * 3,
                                        GL_UNSIGNED_SHORT, nullptr,
                                        static_cast<GLsizei>(masses.size()));
                glBindVertexArray(0);

                rlDisableShader();
            }
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
        const simulation_settings cfg = get_settings();
        // In the planar view a cube is seen face-on and would cover a large part of the layout, so
        // keep the current-state marker close to the normal node scale there.
        const float size = cfg.planar_graph ? cfg.vertex_size * 3.0f : cfg.visited_node_size * 2.0f;
        DrawCube(current_mass, size, size, size, VERTEX_CURRENT_COLOR);
    }

    EndMode3D();
    EndTextureMode();
}

auto renderer::draw_board_pane() const -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    BeginTextureMode(board_target);
    ClearBackground(RAYWHITE);

    gui.draw_board();

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
    DrawTextureRec(board_target.texture,
                   Rectangle(0, 0, board_target.texture.width, -board_target.texture.height),
                   Vector2(0, COMPACT_MENU_HEIGHT), WHITE);
    DrawTextureRec(graph_target.texture,
                   Rectangle(0, 0, graph_target.texture.width, -graph_target.texture.height),
                   Vector2(static_cast<float>(right_pane_x()), COMPACT_MENU_HEIGHT), WHITE);

    // Draw borders
    DrawRectangleLinesEx(Rectangle(0, 0, GetScreenWidth(), COMPACT_MENU_HEIGHT), 1.0f, BLACK);
    DrawRectangleLinesEx(controls_pane_bounds(), 1.0f, BLACK);
    DrawRectangleLinesEx(board_pane_bounds(), 1.0f, BLACK);
    DrawRectangleLinesEx(graph_pane_bounds(), 1.0f, BLACK);

    gui.draw(fps, ups, mass_count, spring_count);

    EndDrawing();
}

auto renderer::renderer_memory() const -> memory_usage
{
    memory_usage usage;
    usage.renderer_cpu = edge_indices.capacity() * sizeof(u32) +
        connections.capacity() * sizeof(std::pair<Vector3, Vector3>) +
        colors.capacity() * sizeof(Color) + node_sizes.capacity() * sizeof(float);

    const auto texture_bytes = [](const RenderTexture& target)
    {
        return static_cast<size_t>(target.texture.width) *
            static_cast<size_t>(target.texture.height) * 4;
    };

    usage.renderer_gpu = edge_position_capacity * sizeof(Vector3) +
        edge_index_capacity * sizeof(u32) + color_vbo_capacity * sizeof(Color) +
        size_vbo_capacity * sizeof(float) + texture_bytes(graph_target) +
        texture_bytes(board_target) + texture_bytes(menu_target);
    return usage;
}

auto renderer::render(const std::vector<Vector3>& masses, const int fps, const int ups,
                      const size_t mass_count, const size_t spring_count) -> void
{
    update_texture_sizes();
    const memory_usage memory = renderer_memory();
    gui.set_renderer_memory(memory.renderer_cpu, memory.renderer_gpu);

    draw_mass_springs(masses);
    draw_board_pane();
    draw_menu();
    draw_textures(fps, ups, mass_count, spring_count);
}
