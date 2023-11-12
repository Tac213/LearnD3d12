#include "hello_triangle.h"
#include "d3d12_helper.h"
#include <d3dcompiler.h>

namespace learn_d3d12
{
    HelloTriangle::HelloTriangle(uint32_t width, uint32_t height, std::string name)
        : D3d12Renderer(width, height, name)
        , _viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))
        , _scissor_rect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height))
        , _rtv_descriptor_size(0) {};

    void HelloTriangle::on_init(HWND hwnd)
    {
        _load_pipeline(hwnd);
        _load_assets();
    }

    void HelloTriangle::on_destroy()
    {
        // Ensure that the GPU is no longer referencing resources that are about to be
        // cleaned up by the destructor.
        _wait_for_previous_frame();

        CloseHandle(_fence_event);
        _fence.Reset();
        _command_list.Reset();
        _vertex_buffer.Reset();
        _pipeline_state.Reset();
        _root_signature.Reset();
        _command_allocator.Reset();
        for (auto& render_target : _render_targets)
        {
            render_target.Reset();
        }
        _rtv_heap.Reset();
        _swap_chain.Reset();
        _command_queue.Reset();
        _device.Reset();
    }

    void HelloTriangle::on_update()
    {
    }

    void HelloTriangle::on_render()
    {
        // Record all the commands we need to render the scene into the command list.
        _populate_command_list();

        // Execute the command list.
        ID3D12CommandList* command_lists[] = {_command_list.Get()};
        _command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

        // Present the frame.
        throw_if_failed(_swap_chain->Present(1, 0));

        _wait_for_previous_frame();
    }

    void HelloTriangle::_load_pipeline(HWND hwnd)
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

        throw_if_failed(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_command_allocator)));
    }

    void HelloTriangle::_load_assets()
    {
        // Create an empty root signature.
        {
            CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc;
            root_signature_desc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            throw_if_failed(D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
            throw_if_failed(_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_root_signature)));
        }

        // Create the pipeline state, which includes compiling and loading shaders.
        {
            ComPtr<ID3DBlob> vertex_shader;
            ComPtr<ID3DBlob> pixel_shader;

#if defined(_DEBUG)
            // Enable better shader debugging with the graphics debugging tools.
            UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
            UINT compile_flags = 0;
#endif

            throw_if_failed(D3DCompileFromFile(L"shader/hello_triangle/shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compile_flags, 0, &vertex_shader, nullptr));
            throw_if_failed(D3DCompileFromFile(L"shader/hello_triangle/shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compile_flags, 0, &pixel_shader, nullptr));

            // Define the vertex input layout.
            D3D12_INPUT_ELEMENT_DESC input_element_descs[] =
                {
                    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

            // Describe and create the graphics pipeline state object (PSO).
            D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
            pso_desc.InputLayout = {input_element_descs, _countof(input_element_descs)};
            pso_desc.pRootSignature = _root_signature.Get();
            pso_desc.VS = CD3DX12_SHADER_BYTECODE(vertex_shader.Get());
            pso_desc.PS = CD3DX12_SHADER_BYTECODE(pixel_shader.Get());
            pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            pso_desc.DepthStencilState.DepthEnable = FALSE;
            pso_desc.DepthStencilState.StencilEnable = FALSE;
            pso_desc.SampleMask = UINT_MAX;
            pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            pso_desc.NumRenderTargets = 1;
            pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            pso_desc.SampleDesc.Count = 1;
            throw_if_failed(_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&_pipeline_state)));
        }

        // Create the command list.
        throw_if_failed(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _command_allocator.Get(), _pipeline_state.Get(), IID_PPV_ARGS(&_command_list)));

        // Command lists are created in the recording state, but there is nothing
        // to record yet. The main loop expects it to be closed, so close it now.
        throw_if_failed(_command_list->Close());

        // Create the vertex buffer.
        {
            // Define the geometry for a triangle.
            Vertex triangle_vertices[] =
                {
                    {{0.0f, 0.25f * aspect_ratio, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{0.25f, -0.25f * aspect_ratio, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{-0.25f, -0.25f * aspect_ratio, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}};

            const UINT vertex_buffer_size = sizeof(triangle_vertices);

            CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_size, D3D12_RESOURCE_FLAG_NONE);

            // Note: using upload heaps to transfer static data like vert buffers is not
            // recommended. Every time the GPU needs it, the upload heap will be marshalled
            // over. Please read up on Default Heap usage. An upload heap is used here for
            // code simplicity and because there are very few verts to actually transfer.
            throw_if_failed(_device->CreateCommittedResource(
                &props,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&_vertex_buffer)));

            ComPtr<ID3D12Resource> staging_buffer;
            CD3DX12_HEAP_PROPERTIES staging_props(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC staging_desc = CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_size);
            throw_if_failed(_device->CreateCommittedResource(&staging_props,
                                                             D3D12_HEAP_FLAG_NONE,
                                                             &staging_desc,
                                                             D3D12_RESOURCE_STATE_GENERIC_READ,
                                                             nullptr,
                                                             IID_PPV_ARGS(&staging_buffer)));
            // Copy the triangle data to the vertex buffer.
            // D3D12_SUBRESOURCE_DATA subresource_data;
            // subresource_data.pData = triangle_vertices;
            // subresource_data.RowPitch = vertex_buffer_size;
            // subresource_data.SlicePitch = subresource_data.RowPitch;
            // UpdateSubresources(_command_list.Get(),
            //                    _vertex_buffer.Get(),
            //                    staging_buffer.Get(),
            //                    0,
            //                    0,
            //                    1,
            //                    &subresource_data);

            UINT8* p_vertex_data_begin;
            CD3DX12_RANGE read_range(0, 0);  // We do not intend to read from this resource on the CPU.
            throw_if_failed(_vertex_buffer->Map(0, &read_range, reinterpret_cast<void**>(&p_vertex_data_begin)));
            memcpy(p_vertex_data_begin, triangle_vertices, sizeof(triangle_vertices));
            _vertex_buffer->Unmap(0, nullptr);

            // auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            //     _vertex_buffer.Get(),
            //     D3D12_RESOURCE_STATE_COPY_DEST,
            //     D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            // _command_list->ResourceBarrier(1, &barrier);

            // Initialize the vertex buffer view.
            _vertex_buffer_view.BufferLocation = _vertex_buffer->GetGPUVirtualAddress();
            _vertex_buffer_view.StrideInBytes = sizeof(Vertex);
            _vertex_buffer_view.SizeInBytes = vertex_buffer_size;
        }

        // Create synchronization objects and wait until assets have been uploaded to the GPU.
        {
            throw_if_failed(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));
            _fence_value = 1;

            // Create an event handle to use for frame synchronization.
            _fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (_fence_event == nullptr)
            {
                throw_if_failed(HRESULT_FROM_WIN32(GetLastError()));
            }

            // Wait for the command list to execute; we are reusing the same command
            // list in our main loop but for now, we just want to wait for setup to
            // complete before continuing.
            _wait_for_previous_frame();
        }
    }

    void HelloTriangle::_populate_command_list()
    {
        // Command list allocators can only be reset when the associated
        // command lists have finished execution on the GPU; apps should use
        // fences to determine GPU execution progress.
        throw_if_failed(_command_allocator->Reset());

        // However, when ExecuteCommandList() is called on a particular command
        // list, that command list can then be reset at any time and must be before
        // re-recording.
        throw_if_failed(_command_list->Reset(_command_allocator.Get(), _pipeline_state.Get()));

        // Set necessary state.
        _command_list->SetGraphicsRootSignature(_root_signature.Get());
        _command_list->RSSetViewports(1, &_viewport);
        _command_list->RSSetScissorRects(1, &_scissor_rect);

        // Indicate that the back buffer will be used as a render target.
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_render_targets[_frame_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        _command_list->ResourceBarrier(1, &barrier);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(_rtv_heap->GetCPUDescriptorHandleForHeapStart(), _frame_index, _rtv_descriptor_size);
        _command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

        // Record commands.
        const float clear_color[] = {0.0f, 0.2f, 0.4f, 1.0f};
        _command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
        _command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        _command_list->IASetVertexBuffers(0, 1, &_vertex_buffer_view);
        _command_list->DrawInstanced(3, 1, 0, 0);

        // Indicate that the back buffer will now be used to present.
        auto after_barrier = CD3DX12_RESOURCE_BARRIER::Transition(_render_targets[_frame_index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        _command_list->ResourceBarrier(1, &after_barrier);

        throw_if_failed(_command_list->Close());
    }

    void HelloTriangle::_wait_for_previous_frame()
    {
        // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
        // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
        // sample illustrates how to use fences for efficient resource usage and to
        // maximize GPU utilization.

        // Signal and increment the fence value.
        const UINT64 fence = _fence_value;
        throw_if_failed(_command_queue->Signal(_fence.Get(), fence));
        _fence_value++;

        // Wait until the previous frame is finished.
        if (_fence->GetCompletedValue() < fence)
        {
            throw_if_failed(_fence->SetEventOnCompletion(fence, _fence_event));
            WaitForSingleObject(_fence_event, INFINITE);
        }

        _frame_index = _swap_chain->GetCurrentBackBufferIndex();
    }
}  // namespace learn_d3d12
