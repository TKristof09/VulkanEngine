#pragma once
#include "ECS/Types.hpp"

class IEvent
{
public:
	virtual ~IEvent() {};
	virtual const EventTypeID GetEventTypeID() const = 0;

};
