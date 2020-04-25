#pragma once
#include "ECS/Types.hpp"

class IEvent
{
public:
	IEvent() {};
	virtual ~IEvent() {};
	virtual const EventTypeID GetEventTypeID() const = 0;

};
