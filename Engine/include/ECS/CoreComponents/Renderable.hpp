#pragma once

#include "ECS/Component.hpp"
#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include <vector>

struct Renderable : public Component<Renderable>
{
    std::vector<CommandBuffer> commandBuffers;
    std::vector<CommandBuffer> prePassCommandBuffers;
    Buffer vertexBuffer;
    Buffer indexBuffer;

    Renderable(const std::vector<CommandBuffer>& cbs, const std::vector<CommandBuffer>& prePassCbs, const Buffer& vb, const Buffer& ib):
        commandBuffers(cbs),
		prePassCommandBuffers(prePassCbs),
        vertexBuffer(vb),
        indexBuffer(ib) {};
};
