#pragma once

#include "Core/Events/IEvent.hpp"
#include "Utils/TypeIDManager.hpp"

template<typename T>
class Event : public IEvent
{
public:
	virtual const EventTypeID GetEventTypeID() const override { return STATIC_EVENT_TYPE_ID; }
	static const EventTypeID STATIC_EVENT_TYPE_ID;
};

template<typename T>
const EventTypeID Event<T>::STATIC_EVENT_TYPE_ID = TypeIDManager<IEvent>::Get<T>();