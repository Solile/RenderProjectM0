#pragma once

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>

namespace m0::rhi {

using Microsoft::WRL::ComPtr;

constexpr uint32_t kFramesInFlight = 2;  // DESIGN §5.2
constexpr uint32_t kBackBufferCount = 2;
constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

// Device, direct queue + fence, swap chain, and the per-frame command recording loop.
//
// Frame slots and back buffers are separate things: a slot (command allocator + fence value)
// is reused every kFramesInFlight frames; the back buffer comes from the swap chain.
class Device {
public:
    Device(HWND hwnd, uint32_t width, uint32_t height, bool debugLayer);
    ~Device();  // waits for the GPU before releasing anything
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // Waits until this frame slot's previous work has finished, then returns an open command list.
    ID3D12GraphicsCommandList* beginFrame();
    // Closes and submits the command list, presents, and signals the slot's fence.
    void endFrame(bool vsync);

    ID3D12Resource* backBuffer() const { return backBuffers_[backBufferIndex_].Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv() const;

    void resize(uint32_t width, uint32_t height);
    void waitIdle();

    // Prints debug-layer messages queued since the last call and adds them to the counts.
    void drainDebugMessages();
    uint32_t debugErrorCount() const { return debugErrors_; }
    uint32_t debugWarningCount() const { return debugWarnings_; }

    const std::string& adapterName() const { return adapterName_; }
    // Full path of the D3D12Core.dll actually loaded (shows whether the Agility SDK is in use).
    const std::string& runtimePath() const { return runtimePath_; }

    ID3D12Device* d3d() const { return device_.Get(); }
    ID3D12CommandQueue* queue() const { return queue_.Get(); }
    // Frame slot being recorded, valid between beginFrame and endFrame.
    uint32_t frameSlot() const { return slot_; }
    // How long the last beginFrame blocked waiting for its slot's fence.
    double lastFenceWaitMs() const { return lastFenceWaitMs_; }

private:
    void createBackBufferViews();
    void waitForFenceValue(uint64_t value);

    ComPtr<IDXGIFactory6> factory_;
    ComPtr<IDXGIAdapter1> adapter_;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12InfoQueue> infoQueue_;  // null when the debug layer is off
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<IDXGISwapChain3> swapChain_;
    ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    ComPtr<ID3D12Resource> backBuffers_[kBackBufferCount];
    ComPtr<ID3D12CommandAllocator> allocators_[kFramesInFlight];
    ComPtr<ID3D12GraphicsCommandList> cmd_;
    ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_ = nullptr;

    uint64_t lastSignaled_ = 0;
    uint64_t slotFenceValues_[kFramesInFlight] = {};
    uint32_t slot_ = 0;
    double lastFenceWaitMs_ = 0.0;
    uint32_t backBufferIndex_ = 0;
    uint32_t rtvStride_ = 0;
    bool tearingSupported_ = false;

    uint32_t debugErrors_ = 0;
    uint32_t debugWarnings_ = 0;
    std::string adapterName_;
    std::string runtimePath_;
};

}  // namespace m0::rhi
