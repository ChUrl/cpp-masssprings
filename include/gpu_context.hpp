#ifndef GPU_CONTEXT_HPP_
#define GPU_CONTEXT_HPP_

#include <string>

struct GLFWwindow;

namespace gpu
{
// Startup information for the optional OpenGL compute context. The strings are only written while
// the context is initialized and are then read-only.
struct status
{
    std::string version;
    std::string renderer;
    std::string error;
};

[[nodiscard]] auto runtime_status() -> const status&;
[[nodiscard]] auto available() -> bool;
[[nodiscard]] auto active() -> bool;
auto set_active(bool value) -> void;
} // namespace gpu

// Owns a hidden OpenGL 4.3 context that lives on the physics thread. The visible raylib context
// stays on the render thread; both contexts share GL objects but no GL state.
class gpu_context
{
private:
    GLFWwindow* window_ = nullptr;

public:
    gpu_context() = default;
    ~gpu_context();

    gpu_context(const gpu_context&) = delete;
    auto operator=(const gpu_context&) -> gpu_context& = delete;
    gpu_context(gpu_context&&) = delete;
    auto operator=(gpu_context&&) -> gpu_context& = delete;

    // Must be called on the main thread, after raylib has initialized GLFW.
    auto init(GLFWwindow* share) -> bool;

    // May be called from any thread. GLFW explicitly allows moving a context between threads.
    auto make_current() -> bool;
    auto release_current() -> void;

    // Must be called on the main thread after the physics thread has released the context.
    auto shutdown() -> void;

    [[nodiscard]] auto ok() const -> bool
    {
        return window_ != nullptr;
    }

    [[nodiscard]] auto window() const -> GLFWwindow*
    {
        return window_;
    }
};

#endif
