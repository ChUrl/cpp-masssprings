#ifndef RENDERER_HPP_
#define RENDERER_HPP_

#include "camera.hpp"
#include "config.hpp"
#include "input.hpp"
#include "state_manager.hpp"
#include "user_interface.hpp"

#include <raylib.h>
#include <rlgl.h>

class renderer
{
private:
    const state_manager& state;
    const input_handler& input;
    user_interface& gui;

    const orbit_camera& camera;
    RenderTexture render_target = LoadRenderTexture(GetScreenWidth() / 2, GetScreenHeight() - MENU_HEIGHT);
    RenderTexture klotski_target = LoadRenderTexture(GetScreenWidth() / 2, GetScreenHeight() - MENU_HEIGHT);
    RenderTexture menu_target = LoadRenderTexture(GetScreenWidth(), MENU_HEIGHT);

    // Batching
    std::vector<std::pair<Vector3, Vector3>> connections;

    // Instancing
    static constexpr int INSTANCE_COLOR_ATTR = 5;
    std::vector<Matrix> transforms;
    std::vector<Color> colors;
    Material vertex_mat = LoadMaterialDefault();
    Mesh cube_instance = GenMeshCube(VERTEX_SIZE, VERTEX_SIZE, VERTEX_SIZE);
    Shader instancing_shader = LoadShader("shader/instancing_vertex.glsl", "shader/instancing_fragment.glsl");

    unsigned int color_vbo_id = 0;

public:
    renderer(const orbit_camera& _camera, const state_manager& _state, const input_handler& _input,
             user_interface& _gui)
        : state(_state), input(_input), gui(_gui), camera(_camera)
    {
        instancing_shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(instancing_shader, "mvp");
        instancing_shader.locs[SHADER_LOC_MATRIX_MODEL] =
            GetShaderLocationAttrib(instancing_shader, "instanceTransform");
        instancing_shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(instancing_shader, "viewPos");

        infoln("LOC vertexPosition: {}", rlGetLocationAttrib(instancing_shader.id, "vertexPosition"));
        infoln("LOC instanceTransform: {}", rlGetLocationAttrib(instancing_shader.id, "instanceTransform"));
        infoln("LOC instanceColor: {}", rlGetLocationAttrib(instancing_shader.id, "instanceColor"));

        // vertex_mat.maps[MATERIAL_MAP_DIFFUSE].color = VERTEX_COLOR;
        vertex_mat.shader = instancing_shader;

        transforms.reserve(DRAW_VERTICES_LIMIT);
        colors.reserve(DRAW_VERTICES_LIMIT);
        color_vbo_id = rlLoadVertexBuffer(colors.data(), DRAW_VERTICES_LIMIT * sizeof(Color), true);

        rlEnableVertexArray(cube_instance.vaoId);
        rlEnableVertexBuffer(color_vbo_id);
        rlSetVertexAttribute(INSTANCE_COLOR_ATTR, 4, RL_UNSIGNED_BYTE, true, 0, 0);
        rlEnableVertexAttribute(INSTANCE_COLOR_ATTR);
        rlSetVertexAttributeDivisor(INSTANCE_COLOR_ATTR, 1);

        rlDisableVertexBuffer();
        rlDisableVertexArray();
    }

    renderer(const renderer& copy) = delete;
    auto operator=(const renderer& copy) -> renderer& = delete;
    renderer(renderer&& move) = delete;
    auto operator=(renderer&& move) -> renderer& = delete;

    ~renderer()
    {
        UnloadRenderTexture(render_target);
        UnloadRenderTexture(klotski_target);
        UnloadRenderTexture(menu_target);

        // Instancing
        UnloadMaterial(vertex_mat);
        UnloadMesh(cube_instance);

        // I think the shader already gets unloaded with the material?
        // UnloadShader(instancing_shader);
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
