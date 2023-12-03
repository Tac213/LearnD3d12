#include "braynzar_constant_buffers.h"
#include "d3d12_helper.h"
#include <d3dcompiler.h>
#include <iostream>

namespace learn_d3d12
{
    BraynzarConstantBuffers::BraynzarConstantBuffers(uint32_t width, uint32_t height, std::string name)
        : D3d12Renderer(width, height, name)
        , _viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f)
        , _scissor_rect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height))
        , _rtv_descriptor_size(0)
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
        for (int i = 0; i < kFrameCount; i++)
        {
            _frame_index = i;
            _wait_for_previous_frame();
        }

        // get swapchain out of full screen before exiting
        BOOL fs = false;
        if (_swap_chain->GetFullscreenState(&fs, nullptr))
        {
            _swap_chain->SetFullscreenState(false, nullptr);
        }

        for (auto& descriptor_heap : _main_descriptor_heaps)
        {
            descriptor_heap.Reset();
        }
        _index_buffer.Reset();
        _vertex_buffer.Reset();
        _pipeline_state.Reset();
        _root_signature.Reset();
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
        _depth_stencil_buffer.Reset();
        _dsv_heap.Reset();
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
        // update app logic, such as moving the camera or figuring out what objects are in view
        static float r_increment = 0.00002f;
        static float g_increment = 0.00006f;
        static float b_increment = 0.00009f;

        _cb_color_multiplier_data.color_multiplier.x += r_increment;
        _cb_color_multiplier_data.color_multiplier.y += g_increment;
        _cb_color_multiplier_data.color_multiplier.z += b_increment;

        if (_cb_color_multiplier_data.color_multiplier.x >= 1.0 || _cb_color_multiplier_data.color_multiplier.x <= 0.0)
        {
            _cb_color_multiplier_data.color_multiplier.x = _cb_color_multiplier_data.color_multiplier.x >= 1.0 ? 1.0 : 0.0;
            r_increment = -r_increment;
        }
        if (_cb_color_multiplier_data.color_multiplier.y >= 1.0 || _cb_color_multiplier_data.color_multiplier.y <= 0.0)
        {
            _cb_color_multiplier_data.color_multiplier.y = _cb_color_multiplier_data.color_multiplier.y >= 1.0 ? 1.0 : 0.0;
            g_increment = -g_increment;
        }
        if (_cb_color_multiplier_data.color_multiplier.z >= 1.0 || _cb_color_multiplier_data.color_multiplier.z <= 0.0)
        {
            _cb_color_multiplier_data.color_multiplier.z = _cb_color_multiplier_data.color_multiplier.z >= 1.0 ? 1.0 : 0.0;
            b_increment = -b_increment;
        }

        // copy our ConstantBuffer instance to the mapped constant buffer resource
        memcpy(_cb_color_multiplier_gpu_address[_frame_index], &_cb_color_multiplier_data, sizeof(_cb_color_multiplier_data));
    }

    void BraynzarConstantBuffers::on_render()
    {
        _populate_command_list();

        // Execute the command list.
        ID3D12CommandList* command_lists[] = {_command_list.Get()};
        _command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

        // this command goes in at the end of our command queue. we will know when our command queue
        // has finished because the fence value will be set to "fence_value" from the GPU since the command
        // queue is being executed on the GPU
        throw_if_failed(_command_queue->Signal(_fences[_frame_index].Get(), _fence_values[_frame_index]));

        // Present the frame.
        throw_if_failed(_swap_chain->Present(0, 0));
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

        DXGI_MODE_DESC back_buffer_desc = {};
        back_buffer_desc.Width = width;
        back_buffer_desc.Height = height;
        back_buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

        DXGI_SAMPLE_DESC sample_desc = {};
        sample_desc.Count = 1;

        DXGI_SWAP_CHAIN_DESC swapchain_desc = {};
        swapchain_desc.BufferCount = kFrameCount;
        swapchain_desc.BufferDesc = back_buffer_desc;
        swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapchain_desc.OutputWindow = hwnd;
        swapchain_desc.SampleDesc = sample_desc;
        swapchain_desc.Windowed = true;

        ComPtr<IDXGISwapChain> temp_swap_chain;
        factory->CreateSwapChain(_command_queue.Get(), &swapchain_desc, &temp_swap_chain);
        throw_if_failed(temp_swap_chain.As(&_swap_chain));

        // Describe and create the swap chain.
        // DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
        // swap_chain_desc.BufferCount = kFrameCount;
        // swap_chain_desc.Width = width;
        // swap_chain_desc.Height = height;
        // swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        // swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        // swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        // swap_chain_desc.SampleDesc.Count = 1;

        // ComPtr<IDXGISwapChain1> swap_chain;
        // throw_if_failed(factory->CreateSwapChainForHwnd(
        //     _command_queue.Get(),  // Swap chain needs the queue so that it can force a flush on it.
        //     hwnd,
        //     &swap_chain_desc,
        //     nullptr,
        //     nullptr,
        //     &swap_chain));

        // This sample does not support fullscreen transitions.
        // throw_if_failed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

        // throw_if_failed(swap_chain.As(&_swap_chain));
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
        // Create root signature
        {
            // This is a range of descriptors inside a descriptor heap
            D3D12_DESCRIPTOR_RANGE descriptor_table_ranges[1];                                                    // Only one range right now
            descriptor_table_ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;                               // This is a range of constant buffer views (descriptors)
            descriptor_table_ranges[0].NumDescriptors = 1;                                                        // We only have one constant buffer, so the range is only 1
            descriptor_table_ranges[0].BaseShaderRegister = 0;                                                    // Start index of the shader registers in the range
            descriptor_table_ranges[0].RegisterSpace = 0;                                                         // Space 0. Can usually be zero.
            descriptor_table_ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;  // This appends the range to the end of the root signature descriptor tables

            // Create a descriptor table
            D3D12_ROOT_DESCRIPTOR_TABLE descriptor_table;
            descriptor_table.NumDescriptorRanges = _countof(descriptor_table_ranges);  // We only have one range
            descriptor_table.pDescriptorRanges = &descriptor_table_ranges[0];          // The pointer to the beginning of our ranges array

            // Create a root parameter and fill it out
            D3D12_ROOT_PARAMETER root_parameters[1];                                        // Only one parameter right now
            root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;  // This is a descriptor table
            root_parameters[0].DescriptorTable = descriptor_table;                          // This is our descriptor table for this root parameter
            root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;           // Our pixel shader will be the only shader accessing this parameter for now

            CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc;
            root_signature_desc.Init(_countof(root_parameters),  // We have 1 root parameter
                                     root_parameters,            // A pointer to the beginning of our root parameters array
                                     0,
                                     nullptr,
                                     D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |  // we can deny shader stages here for better performance
                                         D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                                         D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                         D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                                         D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS);

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
            throw_if_failed(D3DCompileFromFile(L"shader/braynzar_constant_buffers/vertex_shader.hlsl", nullptr, nullptr, "main", "vs_5_0", compile_flags, 0, &vertex_shader, nullptr));
            throw_if_failed(D3DCompileFromFile(L"shader/braynzar_constant_buffers/pixel_shader.hlsl", nullptr, nullptr, "main", "ps_5_0", compile_flags, 0, &pixel_shader, nullptr));

            // Fill out a shader bytecode structure, which is basically just a pointer
            // to the shader byte code and the size of the shader bytecode
            D3D12_SHADER_BYTECODE vertex_shader_bytecode = {};
            vertex_shader_bytecode.BytecodeLength = vertex_shader->GetBufferSize();
            vertex_shader_bytecode.pShaderBytecode = vertex_shader->GetBufferPointer();
            D3D12_SHADER_BYTECODE pixel_shader_bytecode = {};
            pixel_shader_bytecode.BytecodeLength = pixel_shader->GetBufferSize();
            pixel_shader_bytecode.pShaderBytecode = pixel_shader->GetBufferPointer();

            // create input layout

            // The input layout is used by the Input Assembler so that it knows
            // how to read the vertex data bound to it.

            D3D12_INPUT_ELEMENT_DESC input_layout[] =
                {
                    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

            // fill out an input layout description structure
            D3D12_INPUT_LAYOUT_DESC input_layout_desc = {};

            // we can get the number of elements in an array by "sizeof(array) / sizeof(arrayElementType)"
            input_layout_desc.NumElements = sizeof(input_layout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
            input_layout_desc.pInputElementDescs = input_layout;

            DXGI_SAMPLE_DESC sample_desc = {};
            sample_desc.Count = 1;  // multisample count (no multisampling, so we just put 1, since we still need 1 sample)

            // create a pipeline state object (PSO)

            // In a real application, you will have many pso's. for each different shader
            // or different combinations of shaders, different blend states or different rasterizer states,
            // different topology types (point, line, triangle, patch), or a different number
            // of render targets you will need a pso

            // VS is the only required shader for a pso. You might be wondering when a case would be where
            // you only set the VS. It's possible that you have a pso that only outputs data with the stream
            // output, and not on a render target, which means you would not need anything after the stream
            // output.

            D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};                         // a structure to define a pso
            pso_desc.InputLayout = input_layout_desc;                                 // the structure describing our input layout
            pso_desc.pRootSignature = _root_signature.Get();                          // the root signature that describes the input data this pso needs
            pso_desc.VS = vertex_shader_bytecode;                                     // structure describing where to find the vertex shader bytecode and how large it is
            pso_desc.PS = pixel_shader_bytecode;                                      // same as VS but for pixel shader
            pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;  // type of topology we are drawing
            pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;                      // format of the render target
            pso_desc.SampleDesc = sample_desc;                                        // must be the same sample description as the swapchain and depth/stencil buffer
            pso_desc.SampleMask = 0xffffffff;                                         // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
            pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);        // a default rasterizer state.
            pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);                  // a default blent state.
            pso_desc.NumRenderTargets = 1;                                            // we are only binding one render target
            pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);   // a default depth stencil state
            pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            throw_if_failed(_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&_pipeline_state)));
        }

        // Create vertex buffer
        ComPtr<ID3D12Resource> vertext_upload_buffer;
        {
            // A quad
            Vertex vertices_data[] = {
                // first quad (closer to camera, blue)
                {-0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f},
                {0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 1.0f, 1.0f},
                {-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f},
                {0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f}};

            int buffer_size = sizeof(vertices_data);

            CD3DX12_HEAP_PROPERTIES buffer_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            CD3DX12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);
            // create default heap
            // default heap is memory on the GPU. Only the GPU has access to this memory
            // To get data into this heap, we will have to upload the data using
            // an upload heap
            _device->CreateCommittedResource(
                &buffer_props,                   // a default heap
                D3D12_HEAP_FLAG_NONE,            // no flags
                &buffer_desc,                    // resource description for a buffer
                D3D12_RESOURCE_STATE_COPY_DEST,  // we will start this heap in the copy destination state since we will copy data
                                                 // from the upload heap to this heap
                nullptr,                         // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
                IID_PPV_ARGS(&_vertex_buffer));

            // we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
            _vertex_buffer->SetName(L"Vertex Buffer Resource Heap");

            CD3DX12_HEAP_PROPERTIES upload_buffer_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC upload_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);
            _device->CreateCommittedResource(
                &upload_buffer_props,               // upload heap
                D3D12_HEAP_FLAG_NONE,               // no flags
                &upload_buffer_desc,                // resource description for a buffer
                D3D12_RESOURCE_STATE_GENERIC_READ,  // GPU will read from this buffer and copy its contents to the default heap
                nullptr,
                IID_PPV_ARGS(&vertext_upload_buffer));
            vertext_upload_buffer->SetName(L"Vertex Buffer Upload Resource Heap");

            // store vertex buffer in upload heap
            D3D12_SUBRESOURCE_DATA vertex_data = {};
            vertex_data.pData = reinterpret_cast<BYTE*>(vertices_data);  // pointer to our vertex array
            vertex_data.RowPitch = buffer_size;                          // size of all our triangle vertex data
            vertex_data.SlicePitch = buffer_size;                        // also the size of our triangle vertex data

            // we are now creating a command with the command list to copy the data from
            // the upload heap to the default heap
            UpdateSubresources(_command_list.Get(), _vertex_buffer.Get(), vertext_upload_buffer.Get(), 0, 0, 1, &vertex_data);

            // transition the vertex buffer data from copy destination state to vertex buffer state
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(_vertex_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            _command_list->ResourceBarrier(1, &barrier);

            // Create a vertex buffer view for the triangles. We get the GPU memory address to the vertex pointer using GetGPUVirtualAddress() method
            _vertex_buffer_view.BufferLocation = _vertex_buffer->GetGPUVirtualAddress();
            _vertex_buffer_view.StrideInBytes = sizeof(Vertex);
            _vertex_buffer_view.SizeInBytes = buffer_size;
        }

        // Create index buffer
        ComPtr<ID3D12Resource> index_upload_buffer;
        {
            // a quad (2 triangles)
            DWORD indices_data[] = {
                // first quad (blue)
                0,
                1,
                2,  // first triangle
                0,
                3,
                1,  // second triangle
            };

            int buffer_size = sizeof(indices_data);

            // create default heap to hold index buffer
            CD3DX12_HEAP_PROPERTIES buffer_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            CD3DX12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);
            _device->CreateCommittedResource(
                &buffer_props,                   // a default heap
                D3D12_HEAP_FLAG_NONE,            // no flags
                &buffer_desc,                    // resource description for a buffer
                D3D12_RESOURCE_STATE_COPY_DEST,  // we will start this heap in the copy destination state since we will copy data
                                                 // from the upload heap to this heap
                nullptr,                         // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
                IID_PPV_ARGS(&_index_buffer));

            // we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
            _index_buffer->SetName(L"Index Buffer Resource Heap");

            // create upload heap to upload index buffer
            CD3DX12_HEAP_PROPERTIES upload_buffer_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC upload_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);
            _device->CreateCommittedResource(
                &upload_buffer_props,               // upload heap
                D3D12_HEAP_FLAG_NONE,               // no flags
                &upload_buffer_desc,                // resource description for a buffer
                D3D12_RESOURCE_STATE_GENERIC_READ,  // GPU will read from this buffer and copy its contents to the default heap
                nullptr,
                IID_PPV_ARGS(&index_upload_buffer));
            index_upload_buffer->SetName(L"Index Buffer Upload Resource Heap");

            // store vertex buffer in upload heap
            D3D12_SUBRESOURCE_DATA index_data = {};
            index_data.pData = reinterpret_cast<BYTE*>(indices_data);  // pointer to our vertex array
            index_data.RowPitch = buffer_size;                         // size of all our triangle vertex data
            index_data.SlicePitch = buffer_size;                       // also the size of our triangle vertex data

            // we are now creating a command with the command list to copy the data from
            // the upload heap to the default heap
            UpdateSubresources(_command_list.Get(), _index_buffer.Get(), index_upload_buffer.Get(), 0, 0, 1, &index_data);

            // transition the vertex buffer data from copy destination state to vertex buffer state
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(_index_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
            _command_list->ResourceBarrier(1, &barrier);

            // Create a index buffer view for the triangles. We get the GPU memory address to the index pointer using GetGPUVirtualAddress() method
            _index_buffer_view.BufferLocation = _index_buffer->GetGPUVirtualAddress();
            _index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
            _index_buffer_view.SizeInBytes = buffer_size;
        }

        // Create the depth/stencil buffer
        {
            // create a depth stencil descriptor heap so we can get a pointer to the depth stencil buffer
            D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {};
            dsv_heap_desc.NumDescriptors = 1;
            dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            throw_if_failed(_device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&_dsv_heap)));

            D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_desc = {};
            depth_stencil_desc.Format = DXGI_FORMAT_D32_FLOAT;
            depth_stencil_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            depth_stencil_desc.Flags = D3D12_DSV_FLAG_NONE;

            D3D12_CLEAR_VALUE depth_optimized_clear_value = {};
            depth_optimized_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
            depth_optimized_clear_value.DepthStencil.Depth = 1.0f;
            depth_optimized_clear_value.DepthStencil.Stencil = 0;

            CD3DX12_HEAP_PROPERTIES buffer_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            CD3DX12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
            _device->CreateCommittedResource(
                &buffer_props,
                D3D12_HEAP_FLAG_NONE,
                &buffer_desc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &depth_optimized_clear_value,
                IID_PPV_ARGS(&_depth_stencil_buffer));
            _dsv_heap->SetName(L"Depth/Stencil Resource Heap");

            _device->CreateDepthStencilView(_depth_stencil_buffer.Get(), &depth_stencil_desc, _dsv_heap->GetCPUDescriptorHandleForHeapStart());
            _depth_stencil_buffer->SetName(L"Depth Stencil Buffer");
        }

        // Create a constant buffer descriptor heap for each frame
        // this is the descriptor heap that will store our constant buffer descriptor
        for (auto& main_descriptor_heap : _main_descriptor_heaps)
        {
            D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
            heap_desc.NumDescriptors = 1;
            heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            _device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&main_descriptor_heap));
        }

        // create the constant buffer resource heap
        // We will update the constant buffer one or more times per frame, so we will use only an upload heap
        // unlike previously we used an upload heap to upload the vertex and index data, and then copied over
        // to a default heap. If you plan to use a resource for more than a couple frames, it is usually more
        // efficient to copy to a default heap where it stays on the gpu. In this case, our constant buffer
        // will be modified and uploaded at least once per frame, so we only use an upload heap

        {
            ComPtr<ID3D12Resource> constant_buffer_upload_heap[kFrameCount];
            // create a resource heap, descriptor heap, and pointer to cbv for each frame
            for (int i = 0; i < kFrameCount; ++i)
            {
                CD3DX12_HEAP_PROPERTIES upload_buffer_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
                CD3DX12_RESOURCE_DESC upload_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64);
                throw_if_failed(_device->CreateCommittedResource(
                    &upload_buffer_props,               // this heap will be used to upload the constant buffer data
                    D3D12_HEAP_FLAG_NONE,               // no flags
                    &upload_buffer_desc,                // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
                    D3D12_RESOURCE_STATE_GENERIC_READ,  // will be data that is read from so we keep it in the generic read state
                    nullptr,                            // we do not have use an optimized clear value for constant buffers
                    IID_PPV_ARGS(&constant_buffer_upload_heap[i])));
                constant_buffer_upload_heap[i]->SetName(L"Constant Buffer Upload Resource Heap");

                D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
                cbv_desc.BufferLocation = constant_buffer_upload_heap[i]->GetGPUVirtualAddress();
                cbv_desc.SizeInBytes = (sizeof(ConstantBuffer) + 255) & ~255;  // CB size is required to be 256-byte aligned.
                _device->CreateConstantBufferView(&cbv_desc, _main_descriptor_heaps[i]->GetCPUDescriptorHandleForHeapStart());

                ZeroMemory(&_cb_color_multiplier_data, sizeof(_cb_color_multiplier_data));

                CD3DX12_RANGE read_range(0, 0);  // We do not intend to read from this resource on the CPU. (End is less than or equal to begin)
                throw_if_failed(constant_buffer_upload_heap[i]->Map(0, &read_range, reinterpret_cast<void**>(&_cb_color_multiplier_gpu_address[i])));
                memcpy(_cb_color_multiplier_gpu_address[i], &_cb_color_multiplier_data, sizeof(_cb_color_multiplier_data));
            }
        }

        // We now execute the command list to upload the initial assets (triangle data).
        _command_list->Close();
        ID3D12CommandList* command_lists[] = {_command_list.Get()};
        _command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

        // increment the fence value now, otherwise the buffer might not be uploaded by the time we start drawing
        _fence_values[_frame_index]++;
        throw_if_failed(_command_queue->Signal(_fences[_frame_index].Get(), _fence_values[_frame_index]));
    }

    void BraynzarConstantBuffers::_populate_command_list()
    {
        // We have to wait for the gpu to finish with the command allocator before we reset it
        _wait_for_previous_frame();

        // we can only reset an allocator once the gpu is done with it
        // resetting an allocator frees the memory that the command list was stored in
        throw_if_failed(_command_allocators[_frame_index]->Reset());

        // reset the command list. by resetting the command list we are putting it into
        // a recording state so we can start recording commands into the command allocator.
        // the command allocator that we reference here may have multiple command lists
        // associated with it, but only one can be recording at any time. Make sure
        // that any other command lists associated to this command allocator are in
        // the closed state (not recording).
        // Here you will pass an initial pipeline state object as the second parameter,
        // but in this tutorial we are only clearing the rtv, and do not actually need
        // anything but an initial default pipeline, which is what we get by setting
        // the second parameter to NULL
        throw_if_failed(_command_list->Reset(_command_allocators[_frame_index].Get(), _pipeline_state.Get()));

        // here we start recording commands into the commandList (which all the commands will be stored in the commandAllocator)

        // transition the "frameIndex" render target from the present state to the render target state so the command list draws to it starting from here
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_render_targets[_frame_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        _command_list->ResourceBarrier(1, &barrier);

        // here we again get the handle to our current render target view so we can set it as the render target in the output merger stage of the pipeline
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(_rtv_heap->GetCPUDescriptorHandleForHeapStart(), _frame_index, _rtv_descriptor_size);

        // get a handle to the depth/stencil buffer
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_handle(_dsv_heap->GetCPUDescriptorHandleForHeapStart());

        // set the render target for the output merger stage (the output of the pipeline)
        _command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);

        // Clear the render target by using the ClearRenderTargetView command
        const float clear_color[] = {0.0f, 0.2f, 0.4f, 1.0f};
        _command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

        // clear the depth/stencil buffer
        _command_list->ClearDepthStencilView(_dsv_heap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        // set root signature
        _command_list->SetGraphicsRootSignature(_root_signature.Get());

        // set constant buffer descriptor heap
        ID3D12DescriptorHeap* descriptor_heaps[] = {_main_descriptor_heaps[_frame_index].Get()};
        _command_list->SetDescriptorHeaps(_countof(descriptor_heaps), descriptor_heaps);

        // set the root descriptor table 0 to the constant buffer descriptor heap
        _command_list->SetGraphicsRootDescriptorTable(0, _main_descriptor_heaps[_frame_index]->GetGPUDescriptorHandleForHeapStart());

        // draw triangle
        _command_list->RSSetViewports(1, &_viewport);
        _command_list->RSSetScissorRects(1, &_scissor_rect);
        _command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        _command_list->IASetVertexBuffers(0, 1, &_vertex_buffer_view);
        _command_list->IASetIndexBuffer(&_index_buffer_view);
        _command_list->DrawIndexedInstanced(6, 1, 0, 0, 0);

        // transition the "_frame_index" render target from the render target state to the present state. If the debug layer is enabled, you will receive a
        // warning if present is called on the render target when it's not in the present state
        auto after_barrier = CD3DX12_RESOURCE_BARRIER::Transition(_render_targets[_frame_index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        _command_list->ResourceBarrier(1, &after_barrier);

        throw_if_failed(_command_list->Close());
    }

    void BraynzarConstantBuffers::_wait_for_previous_frame()
    {
        // swap the current rtv buffer index so we draw on the correct buffer
        _frame_index = _swap_chain->GetCurrentBackBufferIndex();

        // if the current fence value is still less than "_fence_value", then we know the GPU has not finished executing
        // the command queue since it has not reached the "_command_queue->Signal(_fence, _fence_value)" command
        if (_fences[_frame_index]->GetCompletedValue() < _fence_values[_frame_index])
        {
            // we have the fence create an event which is signaled once the fence's current value is "fence_value"
            throw_if_failed(_fences[_frame_index]->SetEventOnCompletion(_fence_values[_frame_index], _fence_event));

            // We will wait until the fence has triggered the event that it's current value has reached "fence_value". once it's value
            // has reached "fence_value", we know the command queue has finished executing
            WaitForSingleObject(_fence_event, INFINITE);
        }

        // increment fence value for next frame
        _fence_values[_frame_index]++;
    }

    void BraynzarConstantBuffers::_debug_message_callback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID message_id, const char* description, void* context)
    {
#if defined(_DEBUG)
        switch (severity)
        {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                std::cerr << "[FATAL]" << description << std::endl;
                break;
            case D3D12_MESSAGE_SEVERITY_ERROR:
                std::cerr << "[ERROR]" << description << std::endl;
                break;
            case D3D12_MESSAGE_SEVERITY_WARNING:
                std::cerr << "[WARNING]" << description << std::endl;
                break;
            case D3D12_MESSAGE_SEVERITY_INFO:
                std::cout << "[INFO]" << description << std::endl;
                break;
            case D3D12_MESSAGE_SEVERITY_MESSAGE:
                std::cout << "[MESSAGE]" << description << std::endl;
                break;
        }
#endif
    }
}  // namespace learn_d3d12
