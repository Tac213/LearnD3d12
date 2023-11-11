#pragma once

#include "application.h"
#include <GLFW/glfw3.h>

namespace learn_d3d12
{
    class GlfwApplication : public Application
    {
    public:
        GlfwApplication() = default;
        virtual ~GlfwApplication() override;
        virtual int exec(std::shared_ptr<D3d12Renderer> renderer) override;

    private:
        GLFWwindow* _window;
        void _shutdown();
    };
}  // namespace learn_d3d12
