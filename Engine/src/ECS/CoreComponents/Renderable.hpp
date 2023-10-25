#pragma once

#include "ECS/Component.hpp"
#include "Rendering/Buffer.hpp"
#include "Rendering/CommandBuffer.hpp"
#include <vector>

struct Renderable : public Component<Renderable>
{
    // TODO remove
    Buffer vertexBuffer;
    Buffer indexBuffer;

    uint32_t vertexOffset;
    uint32_t vertexCount;
    uint32_t indexOffset;
    uint32_t indexCount;

    uint32_t objectID;
};
