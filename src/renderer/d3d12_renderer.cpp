#include "d3d12_renderer.h"
#include "braynzar_constant_buffers.h"
#include "hello_triangle.h"
#include <wrl.h>

using Microsoft::WRL::ComPtr;

namespace learn_d3d12
{
    D3d12Renderer::D3d12Renderer(uint32_t width, uint32_t height, std::string name)
        : width(width)
        , height(height)
        , name(name)
        , use_warp_device(false)
    {
        aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
    }

    std::shared_ptr<D3d12Renderer> D3d12Renderer::create(std::string app_type, uint32_t width, uint32_t height, std::string name)
    {
        std::shared_ptr<D3d12Renderer> renderer = nullptr;
        if (app_type == "HelloTriangle")
        {
            renderer = std::make_shared<HelloTriangle>(width, height, name);
        }
        if (app_type == "BraynzarConstantBuffers")
        {
            renderer = std::make_shared<BraynzarConstantBuffers>(width, height, name);
        }
        return renderer;
    }

    void D3d12Renderer::get_hardware_adapter(IDXGIFactory1* factory, IDXGIAdapter1** adapter, bool request_high_performance_adapter)
    {
        *adapter = nullptr;

        ComPtr<IDXGIAdapter1> adapter1;

        ComPtr<IDXGIFactory6> factory6;
        if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6))))
        {
            for (
                UINT adapter_index = 0;
                SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                    adapter_index,
                    request_high_performance_adapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                    IID_PPV_ARGS(&adapter1)));
                ++adapter_index)
            {
                DXGI_ADAPTER_DESC1 desc;
                adapter1->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    // If you want a software adapter, pass in "/warp" on the command line.
                    continue;
                }

                // Check to see whether the adapter supports Direct3D 12, but don't create the
                // actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(adapter1.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        if (adapter1.Get() == nullptr)
        {
            for (UINT adapter_index = 0; SUCCEEDED(factory->EnumAdapters1(adapter_index, &adapter1)); ++adapter_index)
            {
                DXGI_ADAPTER_DESC1 desc;
                adapter1->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    // If you want a software adapter, pass in "/warp" on the command line.
                    continue;
                }

                // Check to see whether the adapter supports Direct3D 12, but don't create the
                // actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(adapter1.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        *adapter = adapter1.Detach();
    }
}  // namespace learn_d3d12
