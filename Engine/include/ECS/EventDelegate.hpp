#pragma once

#include "ECS/IEventDelegate.hpp"

template<typename Class, typename EventType>
class EventDelegate : public IEventDelegate
{
typedef void(Class::*Callback)(const EventType* const);

public:
	EventDelegate(Class* receiver, Callback* callback):
	m_receiver(receiver),
	m_callback(callback) {}

	virtual	void Invoke(const IEvent* const e) override
	{
		(m_receiver->*m_callback)(e);
	}

	virtual bool operator==(const IEventDelegate* other) const override
	{
		EventDelegate* delegate = (EventDelegate*)other;
		if(other == nullptr)
			return false;
		return ((m_callback == delegate->m_callback) && (m_receiver == delegate->m_receiver));
	}
private:
	Class* m_receiver;
	Callback* m_callback;
};
