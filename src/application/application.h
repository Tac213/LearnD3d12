#pragma once

#include <memory>
#include <string>

namespace learn_d3d12
{
    class D3d12Renderer;
    class Application
    {
    public:
        enum class ApplicationType
        {
            kGlfw = 0,
            kWin32 = 1,
        };
        virtual ~Application() = default;
        virtual int exec(std::shared_ptr<D3d12Renderer> renderer) = 0;

        static std::unique_ptr<Application> create(ApplicationType app_type);
        static std::unique_ptr<Application> create(std::string app_type);
    };
}  // namespace learn_d3d12
