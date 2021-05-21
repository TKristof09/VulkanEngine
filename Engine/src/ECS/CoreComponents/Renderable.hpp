#pragma once

#include "ECS/Component.hpp"
#include "Rendering/Buffer.hpp"
#include "Rendering/CommandBuffer.hpp"
#include <vector>

struct Renderable : public Component<Renderable>
{
    Buffer vertexBuffer;
    Buffer indexBuffer;
};
