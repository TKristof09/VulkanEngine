#pragma once

#include "ECS/IEvent.hpp"
#include "ECS/Util.hpp"

template<typename T>
class Event : public IEvent
{
public:
	virtual const EventTypeID GetEventTypeID() const override { return STATIC_EVENT_TYPE_ID; }
private:
	static const EventTypeID STATIC_EVENT_TYPE_ID;
};

template<typename T>
const EventTypeID Event<T>::STATIC_EVENT_TYPE_ID = Util::TypeIDManager<IEvent>::Get<T>();
