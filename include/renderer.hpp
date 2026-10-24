#ifndef RENDERER_HPP_
#define RENDERER_HPP_

#include "config.hpp"
#include "input_handler.hpp"
#include "memory_usage.hpp"
#include "orbit_camera.hpp"
#include "state_manager.hpp"
#include "user_interface.hpp"

#include <raylib.h>
#include <rlgl.h>

class renderer
{
private:
    const state_manager& state;
    input_handler& input;
    user_interface& gui;

    const orbit_camera& camera;
    RenderTexture graph_target = LoadRenderTexture(right_pane_width(), content_height());

    // TODO: Those should be moved to the user_interface.h
    RenderTexture board_target = LoadRenderTexture(left_pane_width(), content_height());
    RenderTexture menu_target = LoadRenderTexture(GetScreenWidth(), COMPACT_MENU_HEIGHT);

    // Edges
    // The graph topology is static between populates, so the edge list is uploaded once as an
    // index buffer. Only the (small) per-mass position buffer is updated every frame.
    unsigned int edge_vao_id = 0;
    unsigned int edge_position_vbo_id = 0;
    unsigned int edge_index_ebo_id = 0;
    size_t edge_position_capacity = 0;
    size_t edge_index_capacity = 0;
    size_t edge_index_count = 0;
    size_t edge_last_link_count = 0;
    size_t edge_last_generation = 0;
    int edge_last_stride = 0;
    std::vector<u32> edge_indices;
    Shader edge_shader = LoadShader("shader/edge_vertex.glsl", "shader/edge_fragment.glsl");
    int edge_color_loc = -1;
    std::vector<std::pair<Vector3, Vector3>> connections;

    // Vertex instancing
    static constexpr int INSTANCE_COLOR_ATTR = 5;
    static constexpr int INSTANCE_POSITION_ATTR = 6;
    static constexpr int INSTANCE_SIZE_ATTR = 7;
    std::vector<Color> colors;
    std::vector<float> node_sizes;

    // The per-mass colors only change when the displayed graph state / options change. Rebuilding
    // them every frame costs 431k hash lookups plus the distance copies for large graphs.
    bool colors_valid = false;
    size_t color_mass_count = 0;
    size_t color_total_moves = 0;
    size_t color_starting_index = 0;
    size_t color_target_count = 0;
    size_t color_path_length = 0;
    size_t color_link_count = 0;
    size_t color_collision_mass = 0;
    size_t color_generation = 0;
    bool color_mark_solutions = false;
    bool color_mark_path = false;
    bool color_by_distance_cached = false;

    // Per-node size only depends on the color category and the three size settings.
    bool node_sizes_valid = false;
    size_t size_mass_count = 0;
    float size_vertex_cached = -1.0f;
    float size_solution_cached = -1.0f;
    float size_visited_cached = -1.0f;

    Mesh cube_instance = GenMeshCube(1.0f, 1.0f, 1.0f);
    Shader instancing_shader =
        LoadShader("shader/instancing_vertex.glsl", "shader/instancing_fragment.glsl");

    Mesh quad_instance = GenMeshPlane(1.0f, 1.0f, 1, 1);
    Shader billboard_shader =
        LoadShader("shader/billboard_vertex.glsl", "shader/billboard_fragment.glsl");
    int billboard_right_loc = -1;
    int billboard_up_loc = -1;

    unsigned int color_vbo_id = 0;
    size_t color_vbo_capacity = 0;
    unsigned int size_vbo_id = 0;
    size_t size_vbo_capacity = 0;

public:
    [[nodiscard]] auto renderer_memory() const -> memory_usage;

    renderer(const orbit_camera& _camera, const state_manager& _state, input_handler& _input,
             user_interface& _gui)
        : state(_state), input(_input), gui(_gui), camera(_camera)
    {
        // Edges
        edge_shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(edge_shader, "vertexPosition");
        edge_shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(edge_shader, "mvp");
        edge_shader.locs[SHADER_LOC_COLOR_DIFFUSE] = GetShaderLocation(edge_shader, "colDiffuse");
        edge_color_loc = GetShaderLocation(edge_shader, "colDiffuse");

        // Edge buffers are allocated lazily once the graph size is known (see ensure_edge_buffers)

        // Vertex instancing
        instancing_shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(instancing_shader, "mvp");
        GetShaderLocationAttrib(instancing_shader, "instancePosition");
        GetShaderLocationAttrib(instancing_shader, "instanceColor");
        GetShaderLocationAttrib(instancing_shader, "instanceScale");

        billboard_shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(billboard_shader, "mvp");
        GetShaderLocationAttrib(billboard_shader, "instancePosition");
        GetShaderLocationAttrib(billboard_shader, "instanceColor");
        GetShaderLocationAttrib(billboard_shader, "instanceScale");
        billboard_right_loc = GetShaderLocation(billboard_shader, "cameraRight");
        billboard_up_loc = GetShaderLocation(billboard_shader, "cameraUp");

        colors.reserve(DRAW_VERTICES_LIMIT);
        node_sizes.reserve(DRAW_VERTICES_LIMIT);
        ensure_color_buffer(DRAW_VERTICES_LIMIT);
        ensure_size_buffer(DRAW_VERTICES_LIMIT);
    }

    NO_COPY_NO_MOVE(renderer);

    ~renderer()
    {
        UnloadRenderTexture(graph_target);
        UnloadRenderTexture(board_target);
        UnloadRenderTexture(menu_target);

        // Edges (buffers are created lazily on the first draw)
        if (edge_vao_id != 0) {
            rlUnloadVertexArray(edge_vao_id);
        }
        if (edge_position_vbo_id != 0) {
            rlUnloadVertexBuffer(edge_position_vbo_id);
        }
        if (edge_index_ebo_id != 0) {
            rlUnloadVertexBuffer(edge_index_ebo_id);
        }
        UnloadShader(edge_shader);

        // Instancing
        UnloadMesh(cube_instance);
        UnloadShader(instancing_shader);
        UnloadMesh(quad_instance);
        UnloadShader(billboard_shader);

        if (color_vbo_id != 0) {
            rlUnloadVertexBuffer(color_vbo_id);
        }
        if (size_vbo_id != 0) {
            rlUnloadVertexBuffer(size_vbo_id);
        }
    }

private:
    auto update_texture_sizes() -> void;

    auto ensure_edge_buffers(size_t mass_count, size_t index_count) -> void;
    auto ensure_color_buffer(size_t count) -> void;
    auto ensure_size_buffer(size_t count) -> void;
    auto draw_mass_springs(const std::vector<Vector3>& masses) -> void;
    auto draw_board_pane() const -> void;
    auto draw_menu() const -> void;
    auto draw_textures(int fps, int ups, size_t mass_count, size_t spring_count) const -> void;

public:
    auto render(const std::vector<Vector3>& masses, int fps, int ups, size_t mass_count,
                size_t spring_count) -> void;
};

#endif
