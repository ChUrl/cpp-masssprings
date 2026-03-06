// gpu_force_layout_raylib_rlgl.cpp
//
// Single-file GPU force-directed layout example for raylib + rlgl + OpenGL 4.3 compute shaders.
// Includes:
//   - grid-based repulsion (GPU)
//   - Hooke springs from edge pairs (GPU)
//   - integration (GPU)
//   - point rendering directly from GPU buffer
//
// Requires:
//   - raylib built with GRAPHICS_API_OPENGL_43
//   - desktop OpenGL 4.3+
//   - GL_ARB_shader_atomic_float (present on RTX 4070)
//
// Build example:
//   g++ -std=c++2b -O2 gpu_force_layout_raylib_rlgl.cpp -lraylib -lGL -ldl -lpthread -lm -o
//   gpu_layout
//
// Notes:
//   - This is a practical starter architecture, not a finished engine.
//   - For very large graphs, add multilevel coarsening and a coarse second grid.
//

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

// rlgl exposes OpenGL headers on desktop builds
#ifndef GRAPHICS_API_OPENGL_43
// You can still compile this file, but compute shaders will not work unless raylib/rlgl
// was built with OpenGL 4.3 support.
#endif

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

static void Require(bool cond, const char* msg)
{
    if (!cond) throw std::runtime_error(msg);
}

static bool HasExtension(const char* extName)
{
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint i = 0; i < count; ++i) {
        const char* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, (GLuint)i));
        if (ext && std::strcmp(ext, extName) == 0) return true;
    }
    return false;
}

static unsigned int BuildComputeProgramRlgl(const char* source)
{
    unsigned int shaderId = rlCompileShader(source, RL_COMPUTE_SHADER);
    Require(shaderId != 0, "Failed to compile compute shader");
    unsigned int programId = rlLoadComputeShaderProgram(shaderId);
    Require(programId != 0, "Failed to link compute shader program");
    rlUnloadShader(shaderId);
    return programId;
}

static unsigned int BuildGraphicsProgramRlgl(const char* vs, const char* fs)
{
    unsigned int vsId = rlCompileShader(vs, RL_VERTEX_SHADER);
    Require(vsId != 0, "Failed to compile vertex shader");

    unsigned int fsId = rlCompileShader(fs, RL_FRAGMENT_SHADER);
    Require(fsId != 0, "Failed to compile fragment shader");

    unsigned int progId = rlLoadShaderProgram(vsId, fsId);
    Require(progId != 0, "Failed to link graphics shader program");

    rlUnloadShader(vsId);
    rlUnloadShader(fsId);
    return progId;
}

static void Dispatch1D(unsigned int count, unsigned int localSizeX = 256)
{
    unsigned int groupsX = (count + localSizeX - 1) / localSizeX;
    rlComputeShaderDispatch(groupsX, 1, 1);
}

static Matrix MatrixFromCamera(const Camera3D& cam)
{
    return GetCameraMatrix(cam);
}

//------------------------------------------------------------------------------
// GPU data layouts (std430-friendly)
//------------------------------------------------------------------------------

struct ParticleGPU
{
    float pos_mass[4];  // xyz position, w mass
    float vel_pad[4];   // xyz velocity
    float force_pad[4]; // xyz accumulated force
};
static_assert(sizeof(ParticleGPU) == 48);

struct CellAccumGPU
{
    float sum_mass[4]; // xyz = sum(pos * mass), w = total mass
    std::uint32_t count;
    std::uint32_t pad[3];
};
static_assert(sizeof(CellAccumGPU) == 32);

struct CellFinalGPU
{
    float com_mass[4]; // xyz = center of mass, w = total mass
    std::uint32_t count;
    std::uint32_t pad[3];
};
static_assert(sizeof(CellFinalGPU) == 32);

struct EdgeGPU
{
    std::uint32_t u;
    std::uint32_t v;
    float weight;
    float restLength;
};
static_assert(sizeof(EdgeGPU) == 16);

//------------------------------------------------------------------------------
// Compute shaders
//------------------------------------------------------------------------------

static const char* CS_CLEAR_GRID = R"GLSL(
#version 430
layout(local_size_x = 256) in;

struct CellAccum {
    vec4 sum_mass;
    uvec4 count_pad;
};

layout(std430, binding=0) buffer GridAccumBuffer {
    CellAccum acc[];
};

uniform uint cellCount;

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= cellCount) return;

    acc[id].sum_mass = vec4(0.0);
    acc[id].count_pad = uvec4(0u);
}
)GLSL";

static const char* CS_ACCUM_GRID = R"GLSL(
#version 430
#extension GL_ARB_shader_atomic_float : enable
layout(local_size_x = 256) in;

struct Particle {
    vec4 pos_mass;
    vec4 vel_pad;
    vec4 force_pad;
};

struct CellAccum {
    vec4 sum_mass;
    uvec4 count_pad;
};

layout(std430, binding=0) buffer ParticleBuffer {
    Particle particles[];
};

layout(std430, binding=1) buffer GridAccumBuffer {
    CellAccum acc[];
};

uniform uint particleCount;
uniform vec3 gridMin;
uniform float cellSize;
uniform ivec3 gridDim;

int cellIndex(ivec3 c, ivec3 dim)
{
    return c.x + c.y*dim.x + c.z*dim.x*dim.y;
}

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= particleCount) return;

    vec3 pos = particles[id].pos_mass.xyz;
    float m  = particles[id].pos_mass.w;

    ivec3 c = ivec3(floor((pos - gridMin) / cellSize));
    c = clamp(c, ivec3(0), gridDim - ivec3(1));

    int idx = cellIndex(c, gridDim);

    atomicAdd(acc[idx].sum_mass.x, pos.x * m);
    atomicAdd(acc[idx].sum_mass.y, pos.y * m);
    atomicAdd(acc[idx].sum_mass.z, pos.z * m);
    atomicAdd(acc[idx].sum_mass.w, m);
    atomicAdd(acc[idx].count_pad.x, 1u);
}
)GLSL";

static const char* CS_FINALIZE_GRID = R"GLSL(
#version 430
layout(local_size_x = 256) in;

struct CellAccum {
    vec4 sum_mass;
    uvec4 count_pad;
};

struct CellFinal {
    vec4 com_mass;
    uvec4 count_pad;
};

layout(std430, binding=0) buffer GridAccumBuffer {
    CellAccum acc[];
};

layout(std430, binding=1) buffer GridFinalBuffer {
    CellFinal cell[];
};

uniform uint cellCount;

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= cellCount) return;

    float mass = acc[id].sum_mass.w;
    uint count = acc[id].count_pad.x;

    vec3 com = vec3(0.0);
    if (mass > 0.0) {
        com = acc[id].sum_mass.xyz / mass;
    }

    cell[id].com_mass = vec4(com, mass);
    cell[id].count_pad = uvec4(count, 0u, 0u, 0u);
}
)GLSL";

static const char* CS_REPULSION = R"GLSL(
#version 430
layout(local_size_x = 256) in;

struct Particle {
    vec4 pos_mass;
    vec4 vel_pad;
    vec4 force_pad;
};

struct CellFinal {
    vec4 com_mass;
    uvec4 count_pad;
};

layout(std430, binding=0) buffer ParticleBuffer {
    Particle particles[];
};

layout(std430, binding=1) buffer GridFinalBuffer {
    CellFinal cell[];
};

uniform uint particleCount;
uniform vec3 gridMin;
uniform float cellSize;
uniform ivec3 gridDim;

uniform int radiusCells;
uniform float softening;
uniform float repulsionK2;
uniform float maxRepulsionForce;
uniform float selfCellFactor;

int cellIndex(ivec3 c, ivec3 dim)
{
    return c.x + c.y*dim.x + c.z*dim.x*dim.y;
}

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= particleCount) return;

    vec3 pos = particles[id].pos_mass.xyz;
    ivec3 base = ivec3(floor((pos - gridMin) / cellSize));
    base = clamp(base, ivec3(0), gridDim - ivec3(1));

    vec3 force = vec3(0.0);

    for (int dz = -radiusCells; dz <= radiusCells; ++dz)
    for (int dy = -radiusCells; dy <= radiusCells; ++dy)
    for (int dx = -radiusCells; dx <= radiusCells; ++dx)
    {
        ivec3 c = base + ivec3(dx, dy, dz);
        if (any(lessThan(c, ivec3(0))) || any(greaterThanEqual(c, gridDim))) continue;

        int idx = cellIndex(c, gridDim);
        float mass = cell[idx].com_mass.w;
        if (mass <= 0.0) continue;

        vec3 com = cell[idx].com_mass.xyz;
        vec3 d = pos - com;

        float r2 = dot(d, d) + softening;
        float factor = ((dx == 0) && (dy == 0) && (dz == 0)) ? selfCellFactor : 1.0;

        // k^2 / r magnitude with direction d/r = k^2 * d / r^2
        float invR = inversesqrt(r2);
        force += factor * (repulsionK2 * mass) * d * (invR * invR);
    }

    float len = length(force);
    if (len > maxRepulsionForce) {
        force *= (maxRepulsionForce / len);
    }

    particles[id].force_pad.xyz = force;
}
)GLSL";

static const char* CS_SPRINGS = R"GLSL(
#version 430
#extension GL_ARB_shader_atomic_float : enable
layout(local_size_x = 256) in;

struct Particle {
    vec4 pos_mass;
    vec4 vel_pad;
    vec4 force_pad;
};

struct Edge {
    uint u;
    uint v;
    float weight;
    float restLength;
};

layout(std430, binding=0) buffer ParticleBuffer {
    Particle particles[];
};

layout(std430, binding=1) buffer EdgeBuffer {
    Edge edges[];
};

uniform uint edgeCount;
uniform float springK;
uniform float maxSpringForce;

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= edgeCount) return;

    Edge e = edges[id];
    vec3 pu = particles[e.u].pos_mass.xyz;
    vec3 pv = particles[e.v].pos_mass.xyz;

    vec3 d = pv - pu;
    float dist = length(d) + 1e-6;
    vec3 dir = d / dist;

    // Hooke: F = k * (dist - rest)
    float stretch = dist - e.restLength;
    float mag = springK * e.weight * stretch;
    mag = clamp(mag, -maxSpringForce, maxSpringForce);

    vec3 F = mag * dir;

    atomicAdd(particles[e.u].force_pad.x,  F.x);
    atomicAdd(particles[e.u].force_pad.y,  F.y);
    atomicAdd(particles[e.u].force_pad.z,  F.z);

    atomicAdd(particles[e.v].force_pad.x, -F.x);
    atomicAdd(particles[e.v].force_pad.y, -F.y);
    atomicAdd(particles[e.v].force_pad.z, -F.z);
}
)GLSL";

static const char* CS_INTEGRATE = R"GLSL(
#version 430
layout(local_size_x = 256) in;

struct Particle {
    vec4 pos_mass;
    vec4 vel_pad;
    vec4 force_pad;
};

layout(std430, binding=0) buffer ParticleBuffer {
    Particle particles[];
};

uniform uint particleCount;
uniform float dt;
uniform float damping;
uniform float stepScale;

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= particleCount) return;

    vec3 vel = particles[id].vel_pad.xyz;
    vec3 force = particles[id].force_pad.xyz;

    vel = (vel + dt * force) * damping;
    vec3 pos = particles[id].pos_mass.xyz + (dt * stepScale) * vel;

    particles[id].vel_pad.xyz = vel;
    particles[id].pos_mass.xyz = pos;
    particles[id].force_pad.xyz = vec3(0.0);
}
)GLSL";

//------------------------------------------------------------------------------
// Rendering shaders
//------------------------------------------------------------------------------

static const char* VS_POINTS = R"GLSL(
#version 430
layout(location=0) in vec3 aPos;

uniform mat4 uMVP;
uniform float uPointSize;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    gl_PointSize = uPointSize;
}
)GLSL";

static const char* FS_POINTS = R"GLSL(
#version 430
out vec4 FragColor;

void main()
{
    vec2 p = gl_PointCoord * 2.0 - 1.0;
    if (dot(p, p) > 1.0) discard;
    FragColor = vec4(0.90, 0.95, 1.00, 1.00);
}
)GLSL";

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------

int main()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1400, 900, "raylib rlgl GPU force-directed layout");

    TraceLog(LOG_INFO, "GL_VERSION: %s", glGetString(GL_VERSION));
    TraceLog(LOG_INFO, "GLSL_VERSION: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    try {
        Require(HasExtension("GL_ARB_compute_shader"),
                "Missing GL_ARB_compute_shader. Build raylib with OpenGL 4.3.");
        Require(HasExtension("GL_ARB_shader_storage_buffer_object"),
                "Missing GL_ARB_shader_storage_buffer_object.");
        Require(HasExtension("GL_ARB_shader_atomic_float"), "Missing GL_ARB_shader_atomic_float.");

        //--------------------------------------------------------------------------
        // Simulation parameters
        //--------------------------------------------------------------------------

        const std::uint32_t nodeCount = 30000; // change to 100000 on your machine
        const std::uint32_t approxEdgeCount = nodeCount * 2;

        const int gridX = 64;
        const int gridY = 64;
        const int gridZ = 64;
        const std::uint32_t gridCellCount = std::uint32_t(gridX * gridY * gridZ);

        Vector3 simMin{-40.0f, -40.0f, -40.0f};
        Vector3 simMax{40.0f, 40.0f, 40.0f};
        Vector3 simExtent = Vector3Subtract(simMax, simMin);

        float cellSize = std::max(
            {simExtent.x / float(gridX), simExtent.y / float(gridY), simExtent.z / float(gridZ)});

        float dt = 0.020f;
        float damping = 0.90f;
        float stepScale = 1.0f;
        float repulsionK2 = 0.35f;
        float springK = 0.18f;
        float springRest = 1.2f;
        float softening = 0.02f;
        float maxRepulsionForce = 60.0f;
        float maxSpringForce = 40.0f;
        float selfCellFactor = 0.15f;
        int radiusCells = 2;

        //--------------------------------------------------------------------------
        // CPU-side initial data
        //--------------------------------------------------------------------------

        std::vector<ParticleGPU> particles(nodeCount);
        for (std::uint32_t i = 0; i < nodeCount; ++i) {
            float rx = float(GetRandomValue(-10000, 10000)) / 10000.0f;
            float ry = float(GetRandomValue(-10000, 10000)) / 10000.0f;
            float rz = float(GetRandomValue(-10000, 10000)) / 10000.0f;

            Vector3 p{simMin.x + (rx * 0.5f + 0.5f) * simExtent.x,
                      simMin.y + (ry * 0.5f + 0.5f) * simExtent.y,
                      simMin.z + (rz * 0.5f + 0.5f) * simExtent.z};

            particles[i].pos_mass[0] = p.x;
            particles[i].pos_mass[1] = p.y;
            particles[i].pos_mass[2] = p.z;
            particles[i].pos_mass[3] = 1.0f;

            particles[i].vel_pad[0] = 0.0f;
            particles[i].vel_pad[1] = 0.0f;
            particles[i].vel_pad[2] = 0.0f;
            particles[i].vel_pad[3] = 0.0f;

            particles[i].force_pad[0] = 0.0f;
            particles[i].force_pad[1] = 0.0f;
            particles[i].force_pad[2] = 0.0f;
            particles[i].force_pad[3] = 0.0f;
        }

        std::vector<EdgeGPU> edges;
        edges.reserve(approxEdgeCount);

        // Example spring graph:
        //  - chain edge
        //  - one random edge
        // This gives a graph-like structure instead of pure gas.
        for (std::uint32_t i = 0; i + 1 < nodeCount; ++i) {
            edges.push_back(EdgeGPU{i, i + 1, 1.0f, springRest});
        }

        for (std::uint32_t i = 0; i < nodeCount; ++i) {
            std::uint32_t j = std::uint32_t(GetRandomValue(0, int(nodeCount - 1)));
            if (j != i) {
                edges.push_back(EdgeGPU{i, j, 1.0f, springRest});
            }
        }

        const std::uint32_t edgeCount = (std::uint32_t)edges.size();

        //--------------------------------------------------------------------------
        // GPU buffers via rlgl
        //--------------------------------------------------------------------------

        unsigned int particleSsbo =
            rlLoadShaderBuffer((unsigned int)(particles.size() * sizeof(ParticleGPU)),
                               particles.data(), RL_DYNAMIC_COPY);

        unsigned int gridAccumSsbo = rlLoadShaderBuffer(
            gridCellCount * (unsigned int)sizeof(CellAccumGPU), nullptr, RL_DYNAMIC_COPY);

        unsigned int gridFinalSsbo = rlLoadShaderBuffer(
            gridCellCount * (unsigned int)sizeof(CellFinalGPU), nullptr, RL_DYNAMIC_COPY);

        unsigned int edgeSsbo = rlLoadShaderBuffer((unsigned int)(edges.size() * sizeof(EdgeGPU)),
                                                   edges.data(), RL_STATIC_DRAW);

        Require(particleSsbo != 0, "Failed to create particle SSBO");
        Require(gridAccumSsbo != 0, "Failed to create gridAccum SSBO");
        Require(gridFinalSsbo != 0, "Failed to create gridFinal SSBO");
        Require(edgeSsbo != 0, "Failed to create edge SSBO");

        //--------------------------------------------------------------------------
        // Programs via rlgl
        //--------------------------------------------------------------------------

        unsigned int progClear = BuildComputeProgramRlgl(CS_CLEAR_GRID);
        unsigned int progAccum = BuildComputeProgramRlgl(CS_ACCUM_GRID);
        unsigned int progFinalize = BuildComputeProgramRlgl(CS_FINALIZE_GRID);
        unsigned int progRepel = BuildComputeProgramRlgl(CS_REPULSION);
        unsigned int progSprings = BuildComputeProgramRlgl(CS_SPRINGS);
        unsigned int progIntegrate = BuildComputeProgramRlgl(CS_INTEGRATE);
        unsigned int progPoints = BuildGraphicsProgramRlgl(VS_POINTS, FS_POINTS);

        //--------------------------------------------------------------------------
        // Rendering state
        //--------------------------------------------------------------------------

        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, particleSsbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(ParticleGPU),
                              (const void*)0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindVertexArray(0);

        Camera3D camera{};
        camera.position = {0.0f, 0.0f, 110.0f};
        camera.target = {0.0f, 0.0f, 0.0f};
        camera.up = {0.0f, 1.0f, 0.0f};
        camera.fovy = 60.0f;
        camera.projection = CAMERA_PERSPECTIVE;

        SetTargetFPS(60);

        float orbit = 0.0f;

        //--------------------------------------------------------------------------
        // Main loop
        //--------------------------------------------------------------------------

        while (!WindowShouldClose()) {
            // Simple interactive tuning
            if (IsKeyPressed(KEY_ONE)) radiusCells = 1;
            if (IsKeyPressed(KEY_TWO)) radiusCells = 2;
            if (IsKeyPressed(KEY_THREE)) radiusCells = 3;

            if (IsKeyDown(KEY_Q)) repulsionK2 *= 1.01f;
            if (IsKeyDown(KEY_A)) repulsionK2 *= 0.99f;

            if (IsKeyDown(KEY_W)) springK *= 1.01f;
            if (IsKeyDown(KEY_S)) springK *= 0.99f;

            if (IsKeyDown(KEY_E)) damping = std::min(0.999f, damping + 0.0005f);
            if (IsKeyDown(KEY_D)) damping = std::max(0.70f, damping - 0.0005f);

            if (IsKeyPressed(KEY_SPACE)) stepScale = 1.0f;
            stepScale = std::max(0.10f, stepScale * 0.9994f);

            orbit += 0.0020f;
            camera.position = {110.0f * std::sin(orbit), 35.0f, 110.0f * std::cos(orbit)};

            // Flush raylib draw batch before raw GL / compute work
            rlDrawRenderBatchActive();

            // ------------------------------------------------------------
            // Pass 1: clear grid accum
            // ------------------------------------------------------------
            rlEnableShader(progClear);
            rlBindShaderBuffer(gridAccumSsbo, 0);

            {
                unsigned int u = (unsigned int)gridCellCount;
                int loc = rlGetLocationUniform(progClear, "cellCount");
                rlSetUniform(loc, &u, RL_SHADER_UNIFORM_UINT, 1);
            }

            Dispatch1D(gridCellCount);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // ------------------------------------------------------------
            // Pass 2: accumulate particles into grid
            // ------------------------------------------------------------
            rlEnableShader(progAccum);
            rlBindShaderBuffer(particleSsbo, 0);
            rlBindShaderBuffer(gridAccumSsbo, 1);

            {
                unsigned int u = nodeCount;
                int loc = rlGetLocationUniform(progAccum, "particleCount");
                rlSetUniform(loc, &u, RL_SHADER_UNIFORM_UINT, 1);
            }
            {
                int loc = rlGetLocationUniform(progAccum, "gridMin");
                float v[3] = {simMin.x, simMin.y, simMin.z};
                rlSetUniform(loc, v, RL_SHADER_UNIFORM_VEC3, 1);
            }
            {
                int loc = rlGetLocationUniform(progAccum, "cellSize");
                rlSetUniform(loc, &cellSize, RL_SHADER_UNIFORM_FLOAT, 1);
            }
            {
                int loc = rlGetLocationUniform(progAccum, "gridDim");
                int v[3] = {gridX, gridY, gridZ};
                rlSetUniform(loc, v, RL_SHADER_UNIFORM_IVEC3, 1);
            }

            Dispatch1D(nodeCount);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // ------------------------------------------------------------
            // Pass 3: finalize cell COM
            // ------------------------------------------------------------
            rlEnableShader(progFinalize);
            rlBindShaderBuffer(gridAccumSsbo, 0);
            rlBindShaderBuffer(gridFinalSsbo, 1);

            {
                unsigned int u = (unsigned int)gridCellCount;
                int loc = rlGetLocationUniform(progFinalize, "cellCount");
                rlSetUniform(loc, &u, RL_SHADER_UNIFORM_UINT, 1);
            }

            Dispatch1D(gridCellCount);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // ------------------------------------------------------------
            // Pass 4: repulsion
            // ------------------------------------------------------------
            rlEnableShader(progRepel);
            rlBindShaderBuffer(particleSsbo, 0);
            rlBindShaderBuffer(gridFinalSsbo, 1);

            {
                unsigned int u = nodeCount;
                int loc = rlGetLocationUniform(progRepel, "particleCount");
                rlSetUniform(loc, &u, RL_SHADER_UNIFORM_UINT, 1);
            }
            {
                int loc = rlGetLocationUniform(progRepel, "gridMin");
                float v[3] = {simMin.x, simMin.y, simMin.z};
                rlSetUniform(loc, v, RL_SHADER_UNIFORM_VEC3, 1);
            }
            {
                int loc = rlGetLocationUniform(progRepel, "cellSize");
                rlSetUniform(loc, &cellSize, RL_SHADER_UNIFORM_FLOAT, 1);
            }
            {
                int loc = rlGetLocationUniform(progRepel, "gridDim");
                int v[3] = {gridX, gridY, gridZ};
                rlSetUniform(loc, v, RL_SHADER_UNIFORM_IVEC3, 1);
            }
            {
                int loc = rlGetLocationUniform(progRepel, "radiusCells");
                rlSetUniform(loc, &radiusCells, RL_SHADER_UNIFORM_INT, 1);
            }
            {
                int loc = rlGetLocationUniform(progRepel, "softening");
                rlSetUniform(loc, &softening, RL_SHADER_UNIFORM_FLOAT, 1);
            }
            {
                int loc = rlGetLocationUniform(progRepel, "repulsionK2");
                rlSetUniform(loc, &repulsionK2, RL_SHADER_UNIFORM_FLOAT, 1);
            }
            {
                int loc = rlGetLocationUniform(progRepel, "maxRepulsionForce");
                rlSetUniform(loc, &maxRepulsionForce, RL_SHADER_UNIFORM_FLOAT, 1);
            }
            {
                int loc = rlGetLocationUniform(progRepel, "selfCellFactor");
                rlSetUniform(loc, &selfCellFactor, RL_SHADER_UNIFORM_FLOAT, 1);
            }

            Dispatch1D(nodeCount);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // ------------------------------------------------------------
            // Pass 5: springs
            // ------------------------------------------------------------
            rlEnableShader(progSprings);
            rlBindShaderBuffer(particleSsbo, 0);
            rlBindShaderBuffer(edgeSsbo, 1);

            {
                unsigned int u = edgeCount;
                int loc = rlGetLocationUniform(progSprings, "edgeCount");
                rlSetUniform(loc, &u, RL_SHADER_UNIFORM_UINT, 1);
            }
            {
                int loc = rlGetLocationUniform(progSprings, "springK");
                rlSetUniform(loc, &springK, RL_SHADER_UNIFORM_FLOAT, 1);
            }
            {
                int loc = rlGetLocationUniform(progSprings, "maxSpringForce");
                rlSetUniform(loc, &maxSpringForce, RL_SHADER_UNIFORM_FLOAT, 1);
            }

            Dispatch1D(edgeCount);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // ------------------------------------------------------------
            // Pass 6: integrate
            // ------------------------------------------------------------
            rlEnableShader(progIntegrate);
            rlBindShaderBuffer(particleSsbo, 0);

            {
                unsigned int u = nodeCount;
                int loc = rlGetLocationUniform(progIntegrate, "particleCount");
                rlSetUniform(loc, &u, RL_SHADER_UNIFORM_UINT, 1);
            }
            {
                int loc = rlGetLocationUniform(progIntegrate, "dt");
                rlSetUniform(loc, &dt, RL_SHADER_UNIFORM_FLOAT, 1);
            }
            {
                int loc = rlGetLocationUniform(progIntegrate, "damping");
                rlSetUniform(loc, &damping, RL_SHADER_UNIFORM_FLOAT, 1);
            }
            {
                int loc = rlGetLocationUniform(progIntegrate, "stepScale");
                rlSetUniform(loc, &stepScale, RL_SHADER_UNIFORM_FLOAT, 1);
            }

            Dispatch1D(nodeCount);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

            rlDisableShader();

            // ------------------------------------------------------------
            // Draw
            // ------------------------------------------------------------
            BeginDrawing();
            ClearBackground(Color{9, 11, 16, 255});

            BeginMode3D(camera);
            DrawCubeWiresV(Vector3Scale(Vector3Add(simMin, simMax), 0.5f), simExtent,
                           Fade(GRAY, 0.20f));

            rlDrawRenderBatchActive();

            Matrix view = MatrixFromCamera(camera);
            Matrix proj = MatrixPerspective(DEG2RAD * camera.fovy,
                                            (float)GetScreenWidth() / (float)GetScreenHeight(),
                                            0.1f, 1000.0f);
            Matrix mvp = MatrixMultiply(proj, view);

            glUseProgram(progPoints);
            glBindVertexArray(vao);

            GLint locMvp = glGetUniformLocation(progPoints, "uMVP");
            GLint locPt = glGetUniformLocation(progPoints, "uPointSize");
            glUniformMatrix4fv(locMvp, 1, GL_FALSE, (const float*)&mvp);
            glUniform1f(locPt, 2.0f);

            glEnable(GL_PROGRAM_POINT_SIZE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_DEPTH_TEST);

            glDrawArrays(GL_POINTS, 0, (GLsizei)nodeCount);

            glBindVertexArray(0);
            glUseProgram(0);

            EndMode3D();

            DrawText(TextFormat("nodes=%u edges=%u grid=%dx%dx%d radius=%d", nodeCount, edgeCount,
                                gridX, gridY, gridZ, radiusCells),
                     10, 10, 20, RAYWHITE);
            DrawText(TextFormat("repulsionK2=%.3f  springK=%.3f  damping=%.3f  stepScale=%.3f",
                                repulsionK2, springK, damping, stepScale),
                     10, 36, 20, RAYWHITE);
            DrawText(
                "Keys: 1/2/3 radius, Q/A repulsion, W/S spring, E/D damping, SPACE reset cooling",
                10, 62, 18, Fade(RAYWHITE, 0.8f));

            EndDrawing();
        }

        //--------------------------------------------------------------------------
        // Cleanup
        //--------------------------------------------------------------------------

        glDeleteVertexArrays(1, &vao);

        rlUnloadShaderProgram(progPoints);
        rlUnloadShaderProgram(progClear);
        rlUnloadShaderProgram(progAccum);
        rlUnloadShaderProgram(progFinalize);
        rlUnloadShaderProgram(progRepel);
        rlUnloadShaderProgram(progSprings);
        rlUnloadShaderProgram(progIntegrate);

        rlUnloadShaderBuffer(edgeSsbo);
        rlUnloadShaderBuffer(gridFinalSsbo);
        rlUnloadShaderBuffer(gridAccumSsbo);
        rlUnloadShaderBuffer(particleSsbo);
    }
    catch (const std::exception& e) {
        TraceLog(LOG_ERROR, "%s", e.what());
    }

    CloseWindow();
    return 0;
}
