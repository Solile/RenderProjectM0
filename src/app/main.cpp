// M0 entry point (DESIGN §10).
// T1: window + device + swap chain, clear the back buffer every frame, debug-layer messages counted.
// T2: CPU timing, GPU timestamps per pass, ImGui stats overlay.

#include "platform/clock.h"
#include "platform/window.h"
#include "profile/frame_stats.h"
#include "profile/gpu_timer.h"
#include "profile/overlay.h"
#include "rhi/barriers.h"
#include "rhi/device.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

// Agility SDK: tells the D3D12 loader to use the runtime in .\D3D12\ next to the exe
// instead of the one in System32. Must be exported from the exe itself.
// (Declared first, defined below: GCC warns on an initialized `extern` non-const pointer.)
extern "C" {
__declspec(dllexport) extern const UINT D3D12SDKVersion = M0_AGILITY_SDK_VERSION;
__declspec(dllexport) extern const char* D3D12SDKPath;
}
const char* D3D12SDKPath = ".\\D3D12\\";

namespace {

struct Options {
    uint64_t maxFrames = 0;  // 0 = run until the window is closed
    bool debugLayer = true;
    bool vsync = true;
};

Options parseArgs(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            opt.maxFrames = std::strtoull(argv[++i], nullptr, 10);
        } else if (std::strcmp(argv[i], "--no-debug-layer") == 0) {
            opt.debugLayer = false;
        } else if (std::strcmp(argv[i], "--no-vsync") == 0) {
            opt.vsync = false;
        } else {
            std::fprintf(stderr,
                         "unknown argument: %s\nusage: m0 [--frames N] [--no-debug-layer] [--no-vsync]\n",
                         argv[i]);
            std::exit(2);
        }
    }
    return opt;
}

}  // namespace

int main(int argc, char** argv) {
    const Options opt = parseArgs(argc, argv);
    try {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        m0::Window window(L"M0", 1280, 720);
        // Declared after the window, so it is destroyed first (swap chain before HWND).
        m0::rhi::Device device(window.hwnd(), window.width(), window.height(), opt.debugLayer);

        std::printf("adapter:       %s\n", device.adapterName().c_str());
        std::printf("d3d12 runtime: %s\n", device.runtimePath().c_str());
        std::printf("debug layer:   %s\n", opt.debugLayer ? "on" : "OFF");
        if (device.runtimePath().find("\\D3D12\\D3D12Core.dll") == std::string::npos) {
            std::fprintf(stderr, "warning: Agility SDK runtime not loaded; using the OS runtime\n");
        }

        using m0::profile::GpuPass;
        m0::profile::GpuTimer gpuTimer(device.d3d(), device.queue(), m0::rhi::kFramesInFlight);
        m0::profile::Overlay overlay(window.hwnd(), device.d3d(), device.queue(), m0::rhi::kFramesInFlight,
                                     m0::rhi::kBackBufferFormat);
        window.setMessageHook(&m0::profile::Overlay::handleMessage);

        m0::profile::FrameStats stats;
        const m0::profile::Overlay::Info info{device.adapterName().c_str(), opt.vsync, opt.debugLayer};
        int64_t prevFrameStart = 0;  // 0 = no previous frame to measure against
        uint64_t frame = 0;
        while (window.pumpMessages()) {
            if (window.consumeResized() && !window.minimized()) {
                device.resize(window.width(), window.height());
            }
            if (window.minimized()) {  // nothing to present to
                Sleep(10);
                prevFrameStart = 0;  // don't count the pause as one long frame
                continue;
            }

            const int64_t frameStart = m0::clockNow();
            if (prevFrameStart != 0) stats.cpuFrameMs.add(m0::clockMs(prevFrameStart, frameStart));
            prevFrameStart = frameStart;

            ID3D12GraphicsCommandList* cmd = device.beginFrame();
            stats.fenceWaitMs.add(device.lastFenceWaitMs());
            gpuTimer.beginFrame(cmd, device.frameSlot());  // also reads back this slot's last frame
            stats.addGpu(gpuTimer.lastCompleted());
            overlay.buildStats(stats, info);

            m0::rhi::recordBackBufferToRenderTarget(cmd, device.backBuffer());

            gpuTimer.beginPass(cmd, GpuPass::Clear);
            const float clearColor[4] = {0.10f, 0.20f, 0.35f, 1.0f};
            cmd->ClearRenderTargetView(device.backBufferRtv(), clearColor, 0, nullptr);
            gpuTimer.endPass(cmd, GpuPass::Clear);

            gpuTimer.beginPass(cmd, GpuPass::Overlay);
            overlay.render(cmd, device.backBufferRtv());
            gpuTimer.endPass(cmd, GpuPass::Overlay);

            m0::rhi::recordBackBufferToPresent(cmd, device.backBuffer());
            gpuTimer.endFrame(cmd);
            device.endFrame(opt.vsync);
            stats.cpuWorkMs.add(m0::clockMs(frameStart, m0::clockNow()) - device.lastFenceWaitMs());

            device.drainDebugMessages();
            ++frame;
            if (opt.maxFrames != 0 && frame >= opt.maxFrames) break;
        }

        device.waitIdle();  // before the overlay releases its GPU resources
        device.drainDebugMessages();
        std::printf("last %u frames (avg ms): cpu frame %.3f, cpu work %.3f, fence wait %.3f, "
                    "gpu frame %.3f (clear %.3f, overlay %.3f)\n",
                    m0::profile::kStatWindow, stats.cpuFrameMs.mean(), stats.cpuWorkMs.mean(),
                    stats.fenceWaitMs.mean(), stats.gpuFrameMs.mean(),
                    stats.gpuPassMs[static_cast<uint32_t>(GpuPass::Clear)].mean(),
                    stats.gpuPassMs[static_cast<uint32_t>(GpuPass::Overlay)].mean());
        std::printf("frames: %llu, debug layer: %u errors, %u warnings\n",
                    static_cast<unsigned long long>(frame), device.debugErrorCount(),
                    device.debugWarningCount());
        return device.debugErrorCount() + device.debugWarningCount() == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());
        return 1;
    }
}
