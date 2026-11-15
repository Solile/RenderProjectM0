#include "rhi/device.h"

#include <cstdio>
#include <stdexcept>
#include <vector>

namespace m0::rhi {

namespace {

void check(HRESULT hr, const char* what) {
    if (SUCCEEDED(hr)) return;
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s failed (hr=0x%08lx)", what, static_cast<unsigned long>(hr));
    throw std::runtime_error(buf);
}

std::string toUtf8(const wchar_t* s) {
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, s, -1, out.data(), n, nullptr, nullptr);
    return out;
}

std::string loadedModulePath(const wchar_t* name) {
    HMODULE module = GetModuleHandleW(name);
    if (!module) return {};
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(module, path, MAX_PATH);
    return len ? toUtf8(path) : std::string{};
}

const char* severityName(D3D12_MESSAGE_SEVERITY s) {
    switch (s) {
    case D3D12_MESSAGE_SEVERITY_CORRUPTION: return "CORRUPTION";
    case D3D12_MESSAGE_SEVERITY_ERROR: return "ERROR";
    case D3D12_MESSAGE_SEVERITY_WARNING: return "WARNING";
    case D3D12_MESSAGE_SEVERITY_INFO: return "INFO";
    default: return "MESSAGE";
    }
}

}  // namespace

Device::Device(HWND hwnd, uint32_t width, uint32_t height, bool debugLayer) {
    // The debug layer must be enabled before the device is created.
    if (debugLayer) {
        ComPtr<ID3D12Debug> debug;
        HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
        if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING) {
            // Even the Agility SDK's own d3d12SDKLayers.dll refuses to load without this feature.
            throw std::runtime_error(
                "D3D12 debug layer unavailable: install the Windows optional feature "
                "'Graphics Tools', or run with --no-debug-layer");
        }
        check(hr, "D3D12GetDebugInterface");
        debug->EnableDebugLayer();
    }

    check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory_)), "CreateDXGIFactory2");

    // First hardware adapter, high-performance first. On the laptop that's the only GPU.
    for (UINT i = 0;; ++i) {
        ComPtr<IDXGIAdapter1> candidate;
        if (factory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                 IID_PPV_ARGS(&candidate)) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        DXGI_ADAPTER_DESC1 desc;
        candidate->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (SUCCEEDED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_12_0,
                                        IID_PPV_ARGS(&device_)))) {
            adapter_ = candidate;
            adapterName_ = toUtf8(desc.Description);
            break;
        }
    }
    if (!device_) throw std::runtime_error("no hardware adapter supports D3D12 feature level 12_0");

    runtimePath_ = loadedModulePath(L"D3D12Core.dll");

    if (debugLayer) {
        check(device_.As(&infoQueue_), "QueryInterface(ID3D12InfoQueue)");
        // Only keep messages that count against G2; INFO and MESSAGE are noise here.
        D3D12_MESSAGE_SEVERITY deny[] = {D3D12_MESSAGE_SEVERITY_INFO, D3D12_MESSAGE_SEVERITY_MESSAGE};
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumSeverities = _countof(deny);
        filter.DenyList.pSeverityList = deny;
        check(infoQueue_->PushStorageFilter(&filter), "ID3D12InfoQueue::PushStorageFilter");
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    check(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue_)), "CreateCommandQueue");

    // Tearing is needed for vsync-off presents in windowed mode (DESIGN §5.2).
    BOOL allowTearing = FALSE;
    if (SUCCEEDED(factory_->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing,
                                                sizeof(allowTearing)))) {
        tearingSupported_ = allowTearing == TRUE;
    }

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width = width;
    scDesc.Height = height;
    scDesc.Format = kBackBufferFormat;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = kBackBufferCount;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.Flags = tearingSupported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    ComPtr<IDXGISwapChain1> sc1;
    check(factory_->CreateSwapChainForHwnd(queue_.Get(), hwnd, &scDesc, nullptr, nullptr, &sc1),
          "CreateSwapChainForHwnd");
    check(sc1.As(&swapChain_), "QueryInterface(IDXGISwapChain3)");
    factory_->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = kBackBufferCount;
    check(device_->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rtvHeap_)), "CreateDescriptorHeap(RTV)");
    rtvStride_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    createBackBufferViews();

    for (auto& allocator : allocators_) {
        check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)),
              "CreateCommandAllocator");
    }
    check(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators_[0].Get(), nullptr,
                                     IID_PPV_ARGS(&cmd_)),
          "CreateCommandList");
    check(cmd_->Close(), "ID3D12GraphicsCommandList::Close");  // created open; beginFrame resets it

    check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "CreateFence");
    fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_) throw std::runtime_error("CreateEventW failed");
}

Device::~Device() {
    if (queue_ && fence_ && fenceEvent_) waitIdle();
    if (fenceEvent_) CloseHandle(fenceEvent_);
}

ID3D12GraphicsCommandList* Device::beginFrame() {
    waitForFenceValue(slotFenceValues_[slot_]);
    check(allocators_[slot_]->Reset(), "ID3D12CommandAllocator::Reset");
    check(cmd_->Reset(allocators_[slot_].Get(), nullptr), "ID3D12GraphicsCommandList::Reset");
    backBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
    return cmd_.Get();
}

void Device::endFrame(bool vsync) {
    check(cmd_->Close(), "ID3D12GraphicsCommandList::Close");
    ID3D12CommandList* lists[] = {cmd_.Get()};
    queue_->ExecuteCommandLists(1, lists);

    UINT flags = (!vsync && tearingSupported_) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    HRESULT hr = swapChain_->Present(vsync ? 1 : 0, flags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        check(device_->GetDeviceRemovedReason(), "Present (device removed)");
    }
    check(hr, "IDXGISwapChain::Present");

    check(queue_->Signal(fence_.Get(), ++lastSignaled_), "ID3D12CommandQueue::Signal");
    slotFenceValues_[slot_] = lastSignaled_;
    slot_ = (slot_ + 1) % kFramesInFlight;
}

D3D12_CPU_DESCRIPTOR_HANDLE Device::backBufferRtv() const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(backBufferIndex_) * rtvStride_;
    return h;
}

void Device::resize(uint32_t width, uint32_t height) {
    // The swap chain can't resize while the GPU still references its buffers.
    waitIdle();
    for (auto& bb : backBuffers_) bb.Reset();
    check(swapChain_->ResizeBuffers(kBackBufferCount, width, height, kBackBufferFormat,
                                    tearingSupported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0),
          "IDXGISwapChain::ResizeBuffers");
    createBackBufferViews();
}

void Device::waitIdle() {
    check(queue_->Signal(fence_.Get(), ++lastSignaled_), "ID3D12CommandQueue::Signal");
    waitForFenceValue(lastSignaled_);
}

void Device::drainDebugMessages() {
    if (!infoQueue_) return;
    UINT64 count = infoQueue_->GetNumStoredMessages();
    std::vector<char> storage;
    for (UINT64 i = 0; i < count; ++i) {
        SIZE_T size = 0;
        if (FAILED(infoQueue_->GetMessage(i, nullptr, &size))) continue;
        storage.resize(size);
        auto* msg = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        if (FAILED(infoQueue_->GetMessage(i, msg, &size))) continue;

        if (msg->Severity == D3D12_MESSAGE_SEVERITY_WARNING) ++debugWarnings_;
        else ++debugErrors_;  // the storage filter only lets WARNING, ERROR, CORRUPTION through
        std::fprintf(stderr, "[d3d12 %s] %.*s\n", severityName(msg->Severity),
                     static_cast<int>(msg->DescriptionByteLength), msg->pDescription);
    }
    infoQueue_->ClearStoredMessages();
}

void Device::createBackBufferViews() {
    D3D12_CPU_DESCRIPTOR_HANDLE h = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < kBackBufferCount; ++i) {
        check(swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i])), "IDXGISwapChain::GetBuffer");
        device_->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, h);
        h.ptr += rtvStride_;
    }
    backBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
}

void Device::waitForFenceValue(uint64_t value) {
    if (fence_->GetCompletedValue() >= value) return;
    check(fence_->SetEventOnCompletion(value, fenceEvent_), "ID3D12Fence::SetEventOnCompletion");
    WaitForSingleObject(fenceEvent_, INFINITE);
}

}  // namespace m0::rhi
