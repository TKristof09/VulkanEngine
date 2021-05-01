#pragma once

#include "ECS/Event.hpp"
#include "ECS/Entity.hpp"

struct EntityCreated : public Event<EntityCreated>
{
	Entity entity;
};

// TODO: make this work
struct EntityDestroyed : public Event<EntityDestroyed>
{
	Entity entity;
};
