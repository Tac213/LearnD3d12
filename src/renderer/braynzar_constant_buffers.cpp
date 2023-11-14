#include "braynzar_constant_buffers.h"
#include "d3d12_helper.h"

namespace learn_d3d12
{
    BraynzarConstantBuffers::BraynzarConstantBuffers(uint32_t width, uint32_t height, std::string name)
        : D3d12Renderer(width, height, name)
    {
    }

    void BraynzarConstantBuffers::on_init(HWND hwnd)
    {
        _load_pipeline(hwnd);
        _load_assets();
    }

    void BraynzarConstantBuffers::on_destroy()
    {
        // Ensure that the GPU is no longer referencing resources that are about to be
        // cleaned up by the destructor.
        _wait_for_previous_frame();

        CloseHandle(_fence_event);
        for (auto& fence : _fences)
        {
            fence.Reset();
        }
        _command_list.Reset();
        for (auto& command_allocator : _command_allocators)
        {
            command_allocator.Reset();
        }
        for (auto& render_target : _render_targets)
        {
            render_target.Reset();
        }
        _rtv_heap.Reset();
        _swap_chain.Reset();
        _command_queue.Reset();
        _device.Reset();
    }

    void BraynzarConstantBuffers::on_update()
    {
    }

    void BraynzarConstantBuffers::on_render()
    {
    }

    void BraynzarConstantBuffers::_load_pipeline(HWND hwnd)
    {
        uint32_t dxgi_factory_flags = 0;
#if defined(_DEBUG)
        // Enable the debug layer (requires the Graphics Tools "optional feature").
        // NOTE: Enabling the debug layer after device creation will invalidate the active device.
        {
            ComPtr<ID3D12Debug> debug_controller;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
            {
                debug_controller->EnableDebugLayer();

                // Enable additional debug layers.
                dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }
#endif
        ComPtr<IDXGIFactory4> factory;
        throw_if_failed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory)));

        if (use_warp_device)
        {
            ComPtr<IDXGIAdapter> warp_adapter;
            throw_if_failed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));

            throw_if_failed(D3D12CreateDevice(
                warp_adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&_device)));
        }
        else
        {
            ComPtr<IDXGIAdapter1> hardware_adapter;
            get_hardware_adapter(factory.Get(), &hardware_adapter);

            throw_if_failed(D3D12CreateDevice(
                hardware_adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&_device)));
        }

        // Describe and create the command queue.
        D3D12_COMMAND_QUEUE_DESC queue_desc = {};
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        throw_if_failed(_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&_command_queue)));

        // Describe and create the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
        swap_chain_desc.BufferCount = kFrameCount;
        swap_chain_desc.Width = width;
        swap_chain_desc.Height = height;
        swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swap_chain_desc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> swap_chain;
        throw_if_failed(factory->CreateSwapChainForHwnd(
            _command_queue.Get(),  // Swap chain needs the queue so that it can force a flush on it.
            hwnd,
            &swap_chain_desc,
            nullptr,
            nullptr,
            &swap_chain));

        // This sample does not support fullscreen transitions.
        throw_if_failed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

        throw_if_failed(swap_chain.As(&_swap_chain));
        _frame_index = _swap_chain->GetCurrentBackBufferIndex();

        // Create descriptor heaps.
        {
            // Describe and create a render target view (RTV) descriptor heap.
            D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
            rtv_heap_desc.NumDescriptors = kFrameCount;
            rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            throw_if_failed(_device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&_rtv_heap)));

            _rtv_descriptor_size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        }

        // Create frame resources.
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(_rtv_heap->GetCPUDescriptorHandleForHeapStart());

            // Create a RTV for each frame.
            for (uint32_t n = 0; n < kFrameCount; n++)
            {
                throw_if_failed(_swap_chain->GetBuffer(n, IID_PPV_ARGS(&_render_targets[n])));
                _device->CreateRenderTargetView(_render_targets[n].Get(), nullptr, rtv_handle);
                rtv_handle.Offset(1, _rtv_descriptor_size);
            }
        }

        // Create the command allocators.
        for (auto& command_allocator : _command_allocators)
        {
            throw_if_failed(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)));
        }

        // Create the command list.
        throw_if_failed(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _command_allocators[_frame_index].Get(), nullptr, IID_PPV_ARGS(&_command_list)));

        // Create synchronization objects and wait until assets have been uploaded to the GPU.
        {
            for (uint32_t n = 0; n < kFrameCount; n++)
            {
                throw_if_failed(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fences[n])));
                _fence_values[n] = 0;
            }

            // Create an event handle to use for frame synchronization.
            _fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (_fence_event == nullptr)
            {
                throw_if_failed(HRESULT_FROM_WIN32(GetLastError()));
            }
        }
    }

    void BraynzarConstantBuffers::_load_assets()
    {
    }

    void BraynzarConstantBuffers::_populate_command_list()
    {
    }

    void BraynzarConstantBuffers::_wait_for_previous_frame()
    {
    }
}  // namespace learn_d3d12
