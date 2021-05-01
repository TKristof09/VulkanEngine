#pragma once

#include "Core/Events/Event.hpp"

template<typename T>
struct SystemAdded : public Event<SystemAdded<T>>
{
	T* ptr;
};

struct NewMaterialTypeAdded : public Event<NewMaterialTypeAdded>
{
	std::string name;
	uint32_t priority;
};


