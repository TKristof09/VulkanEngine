#pragma once

#include "ECS/Component.hpp"
#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include <vector>

struct Renderable : public Component<Renderable>
{
    std::vector<CommandBuffer> commandBuffers;
    Buffer vertexBuffer;
    Buffer indexBuffer;

    Renderable(const std::vector<CommandBuffer>& cbs, const Buffer& vb, const Buffer& ib):
        commandBuffers(cbs),
        vertexBuffer(vb),
        indexBuffer(ib) {};
};