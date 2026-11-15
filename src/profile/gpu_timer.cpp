#include "profile/gpu_timer.h"

#include <cstdio>
#include <stdexcept>

namespace m0::profile {

namespace {
void check(HRESULT hr, const char* what) {
    if (SUCCEEDED(hr)) return;
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s failed (hr=0x%08lx)", what, static_cast<unsigned long>(hr));
    throw std::runtime_error(buf);
}
}  // namespace

const char* gpuPassName(GpuPass pass) {
    switch (pass) {
    case GpuPass::Clear: return "Clear";
    case GpuPass::Overlay: return "Overlay";
    default: return "?";
    }
}

GpuTimer::GpuTimer(ID3D12Device* device, ID3D12CommandQueue* queue, uint32_t framesInFlight)
    : framesInFlight_(framesInFlight) {
    if (framesInFlight == 0 || framesInFlight > kMaxFramesInFlight) {
        throw std::runtime_error("GpuTimer: unsupported frames-in-flight count");
    }
    const uint32_t totalQueries = kQueriesPerFrame * framesInFlight;

    D3D12_QUERY_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    heapDesc.Count = totalQueries;
    check(device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&heap_)), "CreateQueryHeap(TIMESTAMP)");

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = sizeof(uint64_t) * totalQueries;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    // Readback-heap resources must be created in COPY_DEST and can never leave it,
    // so this buffer needs no barriers.
    check(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                          D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                          IID_PPV_ARGS(&readback_)),
          "CreateCommittedResource(timestamp readback)");

    // Mapped once for the program's lifetime; each slot's region is only read after its fence.
    void* ptr = nullptr;
    check(readback_->Map(0, nullptr, &ptr), "Map(timestamp readback)");
    mapped_ = static_cast<const uint64_t*>(ptr);

    UINT64 frequency = 0;  // GPU timestamp ticks per second
    check(queue->GetTimestampFrequency(&frequency), "GetTimestampFrequency");
    ticksPerMs_ = static_cast<double>(frequency) / 1000.0;
}

void GpuTimer::beginFrame(ID3D12GraphicsCommandList* cmd, uint32_t slot) {
    slot_ = slot;

    // Safe to read: the caller already waited on this slot's fence.
    const uint32_t mask = slotMasks_[slot];
    const uint64_t* t = mapped_ + slot * kQueriesPerFrame;
    auto has = [mask](uint32_t q) { return (mask & (1u << q)) != 0; };

    last_ = {};
    if (has(kFrameBegin) && has(kFrameEnd)) {
        last_.valid = true;
        last_.frameMs = ticksToMs(t[kFrameBegin], t[kFrameEnd]);
        for (uint32_t p = 0; p < kGpuPassCount; ++p) {
            const auto pass = static_cast<GpuPass>(p);
            if (has(passBegin(pass)) && has(passEnd(pass))) {
                last_.passMs[p] = ticksToMs(t[passBegin(pass)], t[passEnd(pass)]);
            }
        }
    }

    writtenMask_ = 0;
    stamp(cmd, kFrameBegin);
}

void GpuTimer::beginPass(ID3D12GraphicsCommandList* cmd, GpuPass pass) { stamp(cmd, passBegin(pass)); }

void GpuTimer::endPass(ID3D12GraphicsCommandList* cmd, GpuPass pass) { stamp(cmd, passEnd(pass)); }

void GpuTimer::endFrame(ID3D12GraphicsCommandList* cmd) {
    stamp(cmd, kFrameEnd);

    // Resolve only what was stamped this frame: resolving a query that was never written is an error.
    const uint32_t base = slot_ * kQueriesPerFrame;
    for (uint32_t q = 0; q < kQueriesPerFrame; ++q) {
        if (!(writtenMask_ & (1u << q))) continue;
        cmd->ResolveQueryData(heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, base + q, 1, readback_.Get(),
                              sizeof(uint64_t) * (base + q));
    }
    slotMasks_[slot_] = writtenMask_;
}

void GpuTimer::stamp(ID3D12GraphicsCommandList* cmd, uint32_t local) {
    cmd->EndQuery(heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot_ * kQueriesPerFrame + local);
    writtenMask_ |= 1u << local;
}

double GpuTimer::ticksToMs(uint64_t begin, uint64_t end) const {
    return end >= begin ? static_cast<double>(end - begin) / ticksPerMs_ : 0.0;
}

}  // namespace m0::profile
