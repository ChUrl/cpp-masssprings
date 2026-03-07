#ifndef RENDERER_HPP_
#define RENDERER_HPP_

#include "config.hpp"
#include "orbit_camera.hpp"
#include "input_handler.hpp"
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
    RenderTexture graph_target = LoadRenderTexture(GetScreenWidth() / 2, GetScreenHeight() - MENU_HEIGHT);

    // TODO: Those should be moved to the user_interface.h
    RenderTexture klotski_target = LoadRenderTexture(GetScreenWidth() / 2, GetScreenHeight() - MENU_HEIGHT);
    RenderTexture menu_target = LoadRenderTexture(GetScreenWidth(), MENU_HEIGHT);

    // Edges
    unsigned int edge_vao_id = 0;
    unsigned int edge_vbo_id = 0;
    std::vector<Vector3> edge_vertices;
    Shader edge_shader = LoadShader("shader/edge_vertex.glsl", "shader/edge_fragment.glsl");
    int edge_color_loc = -1;
    std::vector<std::pair<Vector3, Vector3>> connections;

    // Vertex instancing
    static constexpr int INSTANCE_COLOR_ATTR = 5;
    std::vector<Matrix> transforms;
    std::vector<Color> colors;
    Material vertex_mat = LoadMaterialDefault();
    Mesh cube_instance = GenMeshCube(VERTEX_SIZE, VERTEX_SIZE, VERTEX_SIZE);
    Shader instancing_shader = LoadShader("shader/instancing_vertex.glsl", "shader/instancing_fragment.glsl");
    unsigned int color_vbo_id = 0;

public:
    // TODO: I am allocating HUGE vertex buffers instead of resizing dynamically...
    //       Edges: 5'000'000 * 2 * 12 Byte ~= 115 MB
    //       Verts: 1'000'000 * 16 Byte ~= 15 MB
    //       This is also allocated on the CPU by the vectors
    renderer(const orbit_camera& _camera, const state_manager& _state, input_handler& _input, user_interface& _gui)
        : state(_state), input(_input), gui(_gui), camera(_camera)
    {
        // Edges
        edge_shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(edge_shader, "vertexPosition");
        edge_shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(edge_shader, "mvp");
        edge_shader.locs[SHADER_LOC_COLOR_DIFFUSE] = GetShaderLocation(edge_shader, "colDiffuse");
        edge_color_loc = GetShaderLocation(edge_shader, "colDiffuse");

        edge_vertices.reserve(DRAW_EDGES_LIMIT * 2);

        edge_vao_id = rlLoadVertexArray();
        edge_vbo_id = rlLoadVertexBuffer(nullptr, DRAW_EDGES_LIMIT * 2 * sizeof(Vector3), true);

        rlEnableVertexArray(edge_vao_id);
        rlEnableVertexBuffer(edge_vbo_id);

        rlSetVertexAttribute(0, 3, RL_FLOAT, false, sizeof(Vector3), 0);
        rlEnableVertexAttribute(0);

        rlDisableVertexBuffer();
        rlDisableVertexArray();

        // Vertex instancing
        instancing_shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(instancing_shader, "mvp");
        instancing_shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(
            instancing_shader,
            "instanceTransform");
        instancing_shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(instancing_shader, "viewPos");

        vertex_mat.shader = instancing_shader;

        transforms.reserve(DRAW_VERTICES_LIMIT);
        colors.reserve(DRAW_VERTICES_LIMIT);
        color_vbo_id = rlLoadVertexBuffer(nullptr, DRAW_VERTICES_LIMIT * sizeof(Color), true);

        rlEnableVertexArray(cube_instance.vaoId);
        rlEnableVertexBuffer(color_vbo_id);
        rlSetVertexAttribute(INSTANCE_COLOR_ATTR, 4, RL_UNSIGNED_BYTE, true, 0, 0);
        rlEnableVertexAttribute(INSTANCE_COLOR_ATTR);
        rlSetVertexAttributeDivisor(INSTANCE_COLOR_ATTR, 1);

        rlDisableVertexBuffer();
        rlDisableVertexArray();
    }

    NO_COPY_NO_MOVE(renderer);

    ~renderer()
    {
        UnloadRenderTexture(graph_target);
        UnloadRenderTexture(klotski_target);
        UnloadRenderTexture(menu_target);

        // Edges
        rlUnloadVertexArray(edge_vao_id);
        rlUnloadVertexBuffer(edge_vbo_id);
        UnloadShader(edge_shader);

        // Instancing
        UnloadMaterial(vertex_mat);
        UnloadMesh(cube_instance);

        // I think the shader already gets unloaded with the material?
        // UnloadShader(instancing_shader);

        rlUnloadVertexBuffer(color_vbo_id);
    }

private:
    auto update_texture_sizes() -> void;

    auto draw_mass_springs(const std::vector<Vector3>& masses) -> void;
    auto draw_klotski() const -> void;
    auto draw_menu() const -> void;
    auto draw_textures(int fps, int ups, size_t mass_count, size_t spring_count) const -> void;

public:
    auto render(const std::vector<Vector3>& masses, int fps, int ups, size_t mass_count, size_t spring_count) -> void;
};

#endif