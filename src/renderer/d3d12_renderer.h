#pragma once

#include <cstdint>
#include <memory>
#include <string>
#ifndef NOMINMAX
#define NOMINMAX  // Avoid compile error
#endif
#include <directx/d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>

namespace learn_d3d12
{
    class D3d12Renderer
    {
    public:
        D3d12Renderer(uint32_t width, uint32_t height, std::string name);
        virtual ~D3d12Renderer() = default;
        virtual void on_init(HWND hwnd) = 0;
        virtual void on_update() = 0;
        virtual void on_render() = 0;
        virtual void on_destroy() = 0;

        // Accessors
        uint32_t get_width() const { return width; }
        uint32_t get_height() const { return height; }
        const char* get_name() const { return name.c_str(); }

        static std::shared_ptr<D3d12Renderer> create(std::string app_type, uint32_t width, uint32_t height, std::string name);

    protected:
        uint32_t width;
        uint32_t height;
        float aspect_ratio;
        std::string name;
        bool use_warp_device;

        static void get_hardware_adapter(IDXGIFactory1* factory, IDXGIAdapter1** adapter, bool request_high_performance_adapter = true);
    };
}  // namespace learn_d3d12
