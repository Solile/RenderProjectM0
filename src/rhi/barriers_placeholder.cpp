// PLACEHOLDER — to be replaced by the owner's hand-written version (DESIGN §9).
// AI-written so T1 can run. Don't build on its internals; only the declarations in barriers.h count.

#include "rhi/barriers.h"

namespace m0::rhi {

namespace {
void transition(ID3D12GraphicsCommandList* cmd, ID3D12Resource* resource,
                D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    cmd->ResourceBarrier(1, &barrier);
}
}  // namespace

void recordBackBufferToRenderTarget(ID3D12GraphicsCommandList* cmd, ID3D12Resource* backBuffer) {
    transition(cmd, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void recordBackBufferToPresent(ID3D12GraphicsCommandList* cmd, ID3D12Resource* backBuffer) {
    transition(cmd, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
}

}  // namespace m0::rhi
