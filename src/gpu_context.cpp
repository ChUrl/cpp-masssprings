#include "gpu_context.hpp"
#include "util.hpp"

#include <GL/glew.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <atomic>
#include <format>

#if !defined(GRAPHICS_API_OPENGL_43)
    #error "MassSprings requires raylib to be built with GRAPHICS_API_OPENGL_43"
#endif

namespace
{
gpu::status g_status;
std::atomic<bool> g_active{false};
} // namespace

auto gpu::runtime_status() -> const status&
{
    return g_status;
}

auto gpu::available() -> bool
{
    return !g_status.version.empty();
}

auto gpu::active() -> bool
{
    return g_active.load(std::memory_order_relaxed);
}

auto gpu::set_active(const bool value) -> void
{
    g_active.store(value, std::memory_order_relaxed);
}

auto gpu_context::init(GLFWwindow* share) -> bool
{
    if (window_ != nullptr) {
        return true;
    }

    GLFWwindow* previous = glfwGetCurrentContext();
    if (previous == nullptr) {
        g_status.error = "No current raylib OpenGL context";
        return false;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 0);

    window_ = glfwCreateWindow(1, 1, "MassSprings compute", nullptr, share);
    if (window_ == nullptr) {
        const char* description = nullptr;
        glfwGetError(&description);
        g_status.error =
            description != nullptr ? description : "Could not create OpenGL 4.3 context";
        return false;
    }

    glfwMakeContextCurrent(window_);
    if (glfwGetCurrentContext() != window_) {
        const char* description = nullptr;
        glfwGetError(&description);
        g_status.error =
            description != nullptr ? description : "Could not activate compute context";
        glfwDestroyWindow(window_);
        window_ = nullptr;
        return false;
    }

    glewExperimental = GL_TRUE;
    const GLenum glew_error = glewInit();
    if (glew_error != GLEW_OK) {
        g_status.error = std::format("GLEW init failed: {}",
                                     reinterpret_cast<const char*>(glewGetErrorString(glew_error)));
        glfwMakeContextCurrent(previous);
        glfwDestroyWindow(window_);
        window_ = nullptr;
        return false;
    }

    const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    g_status.version = version != nullptr ? version : "unknown OpenGL version";
    g_status.renderer = renderer != nullptr ? renderer : "unknown renderer";
    g_status.error.clear();

    glfwMakeContextCurrent(previous);
    return true;
}

auto gpu_context::make_current() -> bool
{
    if (window_ == nullptr) {
        return false;
    }

    glfwMakeContextCurrent(window_);
    if (glfwGetCurrentContext() != window_) {
        const char* description = nullptr;
        glfwGetError(&description);
        g_status.error =
            description != nullptr ? description : "Could not activate compute context";
        return false;
    }

    return true;
}

auto gpu_context::release_current() -> void
{
    if (window_ != nullptr && glfwGetCurrentContext() == window_) {
        glfwMakeContextCurrent(nullptr);
    }
}

auto gpu_context::shutdown() -> void
{
    if (window_ == nullptr) {
        return;
    }

    glfwDestroyWindow(window_);
    window_ = nullptr;
    g_status.version.clear();
    g_status.renderer.clear();
    gpu::set_active(false);
}

gpu_context::~gpu_context()
{
    shutdown();
}
