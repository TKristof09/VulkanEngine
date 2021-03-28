#pragma once

#include "ECS/Component.hpp"
#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include <vector>

struct Renderable : public Component<Renderable>
{

    Buffer vertexBuffer;
    Buffer indexBuffer;

    Renderable(const Buffer& vb, const Buffer& ib):
        vertexBuffer(vb),
        indexBuffer(ib) {};
};
