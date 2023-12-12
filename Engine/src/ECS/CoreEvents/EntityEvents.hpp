#pragma once

#include "Core/Events/Event.hpp"
#include "ECS/Entity.hpp"

struct EntityCreated : public Event
{
    Entity entity;
};
