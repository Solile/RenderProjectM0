#include "profile/overlay.h"

#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"

#include <cstdio>
#include <stdexcept>

// Declared inside `#if 0` in imgui_impl_win32.h on purpose; the backend asks apps to declare it.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

namespace m0::profile {

Overlay::Overlay(HWND hwnd, ID3D12Device* device, ID3D12CommandQueue* queue, uint32_t framesInFlight,
                 DXGI_FORMAT rtvFormat) {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = kSrvCapacity;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&srvHeap_)))) {
        throw std::runtime_error("Overlay: CreateDescriptorHeap(SRV) failed");
    }
    srvCpuStart_ = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    srvGpuStart_ = srvHeap_->GetGPUDescriptorHandleForHeapStart();
    srvStride_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    for (uint32_t i = kSrvCapacity; i > 0; --i) freeSrvs_.push_back(i - 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // no imgui.ini next to the exe

    // The process is per-monitor DPI aware, so scale the UI to the monitor's DPI.
    const float dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(dpiScale);
    style.FontScaleDpi = dpiScale;

    if (!ImGui_ImplWin32_Init(hwnd)) throw std::runtime_error("ImGui_ImplWin32_Init failed");

    ImGui_ImplDX12_InitInfo init;
    init.Device = device;
    init.CommandQueue = queue;
    init.NumFramesInFlight = static_cast<int>(framesInFlight);
    init.RTVFormat = rtvFormat;
    init.DSVFormat = DXGI_FORMAT_UNKNOWN;
    init.UserData = this;
    init.SrvDescriptorHeap = srvHeap_.Get();
    init.SrvDescriptorAllocFn = &Overlay::allocSrv;
    init.SrvDescriptorFreeFn = &Overlay::freeSrv;
    if (!ImGui_ImplDX12_Init(&init)) throw std::runtime_error("ImGui_ImplDX12_Init failed");
}

Overlay::~Overlay() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

bool Overlay::handleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp) != 0;
}

void Overlay::buildStats(const FrameStats& stats, const Info& info) {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    const float pad = 10.0f * ImGui::GetStyle().FontScaleDpi;
    ImGui::SetNextWindowPos(ImVec2(pad, pad), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("Stats", nullptr, flags)) {
        ImGui::TextUnformatted(info.adapter);
        ImGui::Text("vsync %s   debug layer %s", info.vsync ? "on" : "off", info.debugLayer ? "on" : "off");
        ImGui::Separator();

        if (ImGui::BeginTable("times", 3, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("ms");
            ImGui::TableSetupColumn("avg");
            ImGui::TableSetupColumn("max");
            ImGui::TableHeadersRow();
            auto row = [](const char* name, const Stat& s) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name);
                ImGui::TableNextColumn();
                if (s.empty()) ImGui::TextUnformatted("-");
                else ImGui::Text("%7.3f", s.mean());
                ImGui::TableNextColumn();
                if (s.empty()) ImGui::TextUnformatted("-");
                else ImGui::Text("%7.3f", s.max());
            };
            row("CPU frame", stats.cpuFrameMs);
            row("CPU work", stats.cpuWorkMs);
            row("fence wait", stats.fenceWaitMs);
            row("GPU frame", stats.gpuFrameMs);
            for (uint32_t p = 0; p < kGpuPassCount; ++p) {
                char name[32];
                std::snprintf(name, sizeof(name), "  %s", gpuPassName(static_cast<GpuPass>(p)));
                row(name, stats.gpuPassMs[p]);
            }
            ImGui::EndTable();
        }
        ImGui::TextDisabled("last %u frames; GPU times are 2 frames old", kStatWindow);
    }
    ImGui::End();
}

void Overlay::render(ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE rtv) {
    ImGui::Render();
    cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[] = {srvHeap_.Get()};
    cmd->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void Overlay::allocSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                       D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
    auto* self = static_cast<Overlay*>(info->UserData);
    IM_ASSERT(!self->freeSrvs_.empty() && "Overlay: out of SRV descriptors");
    const uint32_t i = self->freeSrvs_.back();
    self->freeSrvs_.pop_back();
    cpu->ptr = self->srvCpuStart_.ptr + static_cast<SIZE_T>(i) * self->srvStride_;
    gpu->ptr = self->srvGpuStart_.ptr + static_cast<UINT64>(i) * self->srvStride_;
}

void Overlay::freeSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                      D3D12_GPU_DESCRIPTOR_HANDLE) {
    auto* self = static_cast<Overlay*>(info->UserData);
    self->freeSrvs_.push_back(static_cast<uint32_t>((cpu.ptr - self->srvCpuStart_.ptr) / self->srvStride_));
}

}  // namespace m0::profile
