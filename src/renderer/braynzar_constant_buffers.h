#pragma once

#include "d3d12_renderer.h"
#include <DirectXMath.h>
#include <directx/d3dx12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

namespace learn_d3d12
{
    class BraynzarConstantBuffers : public D3d12Renderer
    {
    public:
        BraynzarConstantBuffers(uint32_t width, uint32_t height, std::string name);
        virtual void on_init(HWND hwnd) override;
        virtual void on_destroy() override;
        virtual void on_update() override;
        virtual void on_render() override;

    private:
        static const uint32_t kFrameCount = 3;

        struct Vertex
        {
            Vertex(float x, float y, float z, float r, float g, float b, float a)
                : position(x, y, z)
                , color(r, g, b, z) {}
            DirectX::XMFLOAT3 position;
            DirectX::XMFLOAT4 color;
        };

        // Pipeline objects
        CD3DX12_VIEWPORT _viewport;
        CD3DX12_RECT _scissor_rect;
        ComPtr<ID3D12Device> _device;
        ComPtr<IDXGISwapChain3> _swap_chain;
        ComPtr<ID3D12Resource> _render_targets[kFrameCount];
        ComPtr<ID3D12CommandAllocator> _command_allocators[kFrameCount];
        ComPtr<ID3D12CommandQueue> _command_queue;
        ComPtr<ID3D12RootSignature> _root_signature;
        ComPtr<ID3D12DescriptorHeap> _rtv_heap;
        ComPtr<ID3D12PipelineState> _pipeline_state;
        ComPtr<ID3D12GraphicsCommandList> _command_list;
        uint32_t _rtv_descriptor_size;

        // App resources
        ComPtr<ID3D12Resource> _vertex_buffer;
        D3D12_VERTEX_BUFFER_VIEW _vertex_buffer_view;
        ComPtr<ID3D12Resource> _index_buffer;
        D3D12_VERTEX_BUFFER_VIEW _index_buffer_view;

        // Synchronization objects
        uint32_t _frame_index;
        HANDLE _fence_event;
        ComPtr<ID3D12Fence> _fences[kFrameCount];
        size_t _fence_values[kFrameCount];

        void _load_pipeline(HWND hwnd);
        void _load_assets();
        void _populate_command_list();
        void _wait_for_previous_frame();
    };
}  // namespace learn_d3d12
