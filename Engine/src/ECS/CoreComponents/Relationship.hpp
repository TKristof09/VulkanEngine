#pragma once

#include "ECS/Component.hpp"
#include "ECS/Types.hpp"

struct Relationship : public Component<Relationship>
{
	EntityID parent = INVALID_ENTITY_ID;

	size_t numChildren = 0;
	EntityID firstChild = INVALID_ENTITY_ID;

	EntityID nextSibling = INVALID_ENTITY_ID;
	EntityID previousSibling = INVALID_ENTITY_ID;
};
