#pragma once

#include "Core/Events/Event.hpp"
#include "ECS/Entity.hpp"

template<typename T>
struct ComponentAdded : public Event
{
    Entity entity;
};

template<typename T>
struct ComponentRemoved : public Event
{
    Entity entity;
};
