#include "application.h"
// clang-format off
#include "win32_application.h"
#include "glfw_application.h"
// clang-format on

namespace learn_d3d12
{

    std::unique_ptr<Application> Application::create(ApplicationType app_type)
    {
        std::unique_ptr<Application> app = nullptr;
        switch (app_type)
        {
            case ApplicationType::kGlfw:
                app = std::make_unique<GlfwApplication>();
                break;
            case ApplicationType::kWin32:
                app = std::make_unique<Win32Application>();
                break;
        }
        return app;
    }

    std::unique_ptr<Application> Application::create(std::string app_type)
    {
        std::unique_ptr<Application> app = nullptr;
        if (app_type == "glfw")
        {
            app = std::make_unique<GlfwApplication>();
        }
        else if (app_type == "win32")
        {
            app = std::make_unique<Win32Application>();
        }
        else
        {
            app = std::make_unique<GlfwApplication>();
        }
        return app;
    }
}  // namespace learn_d3d12
