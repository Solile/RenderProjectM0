#pragma once

#include "profile/frame_stats.h"

#include <d3d12.h>
#include <windows.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

struct ImGui_ImplDX12_InitInfo;  // global, from imgui_impl_dx12.h

namespace m0::profile {

// Dear ImGui stats overlay (DESIGN §7.5): initialization, per-frame UI, and drawing into the back buffer.
// The ImGui DX12 backend uploads its font texture itself, including its own barriers; that is
// third-party code, not part of the owner-written barriers (DESIGN §9).
class Overlay {
public:
    Overlay(HWND hwnd, ID3D12Device* device, ID3D12CommandQueue* queue, uint32_t framesInFlight,
            DXGI_FORMAT rtvFormat);
    ~Overlay();  // the GPU must be idle (call Device::waitIdle first)
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;

    // Window message hook. Returns true if ImGui consumed the message.
    static bool handleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    struct Info {
        const char* adapter;
        bool vsync;
        bool debugLayer;
    };
    // Builds this frame's UI (CPU only).
    void buildStats(const FrameStats& stats, const Info& info);
    // Records the UI into `cmd`, drawing over the render target `rtv`.
    void render(ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE rtv);

private:
    // ImGui asks the app for SRV descriptors (one per texture it creates).
    static void allocSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                         D3D12_GPU_DESCRIPTOR_HANDLE* gpu);
    static void freeSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                        D3D12_GPU_DESCRIPTOR_HANDLE gpu);

    static constexpr uint32_t kSrvCapacity = 16;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;  // shader-visible
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart_ = {};
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart_ = {};
    uint32_t srvStride_ = 0;
    std::vector<uint32_t> freeSrvs_;
};

}  // namespace m0::profile
