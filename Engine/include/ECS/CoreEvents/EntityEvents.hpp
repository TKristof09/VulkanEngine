#pragma once

#include "ECS/Event.hpp"
#include "ECS/Types.hpp"

struct EntityCreated : public Event<EntityCreated>
{
	EntityID entity;
};

// TODO: make this work
struct EntityDestroyed : public Event<EntityDestroyed>
{
	EntityID entity;
};
