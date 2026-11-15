#include "rhi/barriers.h"

namespace m0::rhi {

namespace {
    void transition(ID3D12GraphicsCommandList* cmd,ID3D12Resource* resource, 
        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        cmd->ResourceBarrier(1, &barrier);
    }
}

// PRESENT -> RENDER_TARGET，在向后缓冲区（back buffer）进行任何绘制操作之前调用。
void recordBackBufferToRenderTarget(ID3D12GraphicsCommandList* cmd, ID3D12Resource* backBuffer){
    transition(cmd,backBuffer,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
}

// RENDER_TARGET -> PRESENT，在最后一次绘制操作之后、调用 Present() 之前调用。
void recordBackBufferToPresent(ID3D12GraphicsCommandList* cmd, ID3D12Resource* backBuffer){
    transition(cmd,backBuffer,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);
}

}