#pragma once

using EventTypeID = uint64_t;

class IEvent
{
public:
	virtual ~IEvent() {};
	virtual const EventTypeID GetEventTypeID() const = 0;

};
