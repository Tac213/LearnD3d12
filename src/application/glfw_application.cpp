#include "glfw_application.h"
#include "../logging/log_macros.h"
#include "../renderer/d3d12_renderer.h"

namespace learn_d3d12
{
    static void glfw_error_callback(int error, const char* description)
    {
        LOG_ERROR(LearnD3d12, "GLFW Error ({0}): {1}", error, description);
    }

    GlfwApplication::~GlfwApplication()
    {
        _shutdown();
    }

    int GlfwApplication::exec(std::shared_ptr<D3d12Renderer> renderer)
    {
        glfwSetErrorCallback(glfw_error_callback);
        // glfw: initialize and configure
        // ------------------------------
        glfwInit();
        // Because GLFW was originally designed to create an OpenGL context
        // we need to tell it to not create an OpenGL context with a subsequent call
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        // create window
        _window = glfwCreateWindow(
            static_cast<int>(renderer->get_width()),
            static_cast<int>(renderer->get_height()),
            renderer->get_name(),
            nullptr,
            nullptr);
        if (!_window)
        {
            glfwTerminate();
            LOG_ERROR(LearnD3d12, "Cannot create glfw window.");
            return 1;
        }

        glfwSetWindowUserPointer(_window, renderer.get());

        while (!glfwWindowShouldClose(_window))
        {
            glfwPollEvents();
        }
        return 0;
    }

    void GlfwApplication::_shutdown()
    {
        glfwDestroyWindow(_window);
        glfwTerminate();
    }
}  // namespace learn_d3d12
