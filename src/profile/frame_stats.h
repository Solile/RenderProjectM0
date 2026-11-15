#pragma once

#include "profile/gpu_timer.h"
#include "profile/rolling_stat.h"

namespace m0::profile {

constexpr uint32_t kStatWindow = 60;  // frames averaged in the live overlay
using Stat = RollingStat<kStatWindow>;

// The metrics of DESIGN §5.3, smoothed for display.
struct FrameStats {
    Stat cpuFrameMs;   // frame start to next frame start
    Stat cpuWorkMs;    // frame start to Present returning, minus fence wait
    Stat fenceWaitMs;  // CPU blocked waiting for a free frame slot
    Stat gpuFrameMs;   // first to last timestamp of the frame's command list
    Stat gpuPassMs[kGpuPassCount];

    void addGpu(const GpuFrameTimes& t) {
        if (!t.valid) return;
        gpuFrameMs.add(t.frameMs);
        for (uint32_t p = 0; p < kGpuPassCount; ++p) gpuPassMs[p].add(t.passMs[p]);
    }
};

}  // namespace m0::profile
