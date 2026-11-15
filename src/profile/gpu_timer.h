#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>

namespace m0::profile {

using Microsoft::WRL::ComPtr;

// Passes timed on the GPU. M0 adds Opaque in T3 (DESIGN §5.3).
enum class GpuPass : uint32_t { Clear, Overlay, Count };
constexpr uint32_t kGpuPassCount = static_cast<uint32_t>(GpuPass::Count);
const char* gpuPassName(GpuPass pass);

struct GpuFrameTimes {
    bool valid = false;  // false until a frame has completed in this slot
    double frameMs = 0.0;
    double passMs[kGpuPassCount] = {};
};

// GPU timestamps for one frame: frame begin/end, plus begin/end of each pass.
//
// The GPU writes a timestamp when it reaches that point in the command list, which is long after
// the CPU recorded it. So each frame slot has its own readback region, and a slot's results are
// only read in beginFrame, after Device::beginFrame has waited on that slot's fence. With 2 frames
// in flight, frame N shows the timings of frame N-2.
class GpuTimer {
public:
    GpuTimer(ID3D12Device* device, ID3D12CommandQueue* queue, uint32_t framesInFlight);
    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;

    // Reads what the GPU wrote the last time `slot` was used, then stamps "frame begin".
    void beginFrame(ID3D12GraphicsCommandList* cmd, uint32_t slot);
    void beginPass(ID3D12GraphicsCommandList* cmd, GpuPass pass);
    void endPass(ID3D12GraphicsCommandList* cmd, GpuPass pass);
    // Stamps "frame end" and copies this frame's timestamps into the slot's readback region.
    void endFrame(ID3D12GraphicsCommandList* cmd);

    // Timings read in the latest beginFrame.
    const GpuFrameTimes& lastCompleted() const { return last_; }

private:
    static constexpr uint32_t kFrameBegin = 0;
    static constexpr uint32_t kFrameEnd = 1;
    static constexpr uint32_t kQueriesPerFrame = 2 + 2 * kGpuPassCount;
    static constexpr uint32_t kMaxFramesInFlight = 4;
    static_assert(kQueriesPerFrame <= 32, "written-query masks are 32-bit");

    static uint32_t passBegin(GpuPass p) { return 2 + 2 * static_cast<uint32_t>(p); }
    static uint32_t passEnd(GpuPass p) { return passBegin(p) + 1; }
    void stamp(ID3D12GraphicsCommandList* cmd, uint32_t local);
    double ticksToMs(uint64_t begin, uint64_t end) const;

    ComPtr<ID3D12QueryHeap> heap_;
    ComPtr<ID3D12Resource> readback_;  // READBACK heap; stays in COPY_DEST for its whole life
    const uint64_t* mapped_ = nullptr;
    double ticksPerMs_ = 1.0;
    uint32_t framesInFlight_ = 0;

    uint32_t slot_ = 0;
    uint32_t writtenMask_ = 0;                          // queries stamped in the frame being recorded
    uint32_t slotMasks_[kMaxFramesInFlight] = {};       // queries resolved by each slot's last frame
    GpuFrameTimes last_;
};

}  // namespace m0::profile
