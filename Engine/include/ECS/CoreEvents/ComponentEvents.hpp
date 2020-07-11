#pragma once

#include "ECS/Event.hpp"
#include "ECS/Types.hpp"

template<typename T>
struct ComponentAdded : public Event<ComponentAdded<T>>
{
	EntityID entity;
	T* component;
};

// TODO: figure out a way to make this work with the RemoveAllComponents function of the component manager
// we might not care about that case tho, the RemoveAllComponents should only be called when the entity is destroyed so I don't think we care about its components anyway
template<typename T>
struct ComponentRemoved : public Event<ComponentAdded<T>>
{
	EntityID entity;
};
