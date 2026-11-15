#pragma once

#include <d3d12.h>

namespace m0::rhi {

// Back-buffer state transitions recorded every frame (DESIGN §7.3).
//
// Owner-written part (DESIGN §9), implemented by hand in barriers.cpp.
// Check: the debug layer reports 0 messages, and deleting either call makes it complain
// (see notes/m0/T1-barriers.md).

// PRESENT -> RENDER_TARGET, before anything draws into the back buffer.
void recordBackBufferToRenderTarget(ID3D12GraphicsCommandList* cmd, ID3D12Resource* backBuffer);

// RENDER_TARGET -> PRESENT, after the last draw and before Present().
void recordBackBufferToPresent(ID3D12GraphicsCommandList* cmd, ID3D12Resource* backBuffer);

}  // namespace m0::rhi
