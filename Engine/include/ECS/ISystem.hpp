#pragma once

#include "ECS/Types.hpp"
#include "ECS/EventHandler.hpp"
#include "ECS/EventDelegate.hpp"
#include <forward_list>

class ISystem
{
	friend class SystemManager;
public:
	virtual ~ISystem() {};

	virtual void PreUpdate(float dt) = 0;
	virtual void Update(float dt) = 0;
	virtual void PostUpdate(float dt) = 0;

	virtual const SystemTypeID GetTypeID() const = 0;

	inline bool IsEnabled() const { return m_enabled; }

protected:
	template<typename EventType, typename... Args>
	void SendEvent(Args... args)
	{
		m_eventHandler->Send<EventType>(std::forward<Args>(args)...);
	}

	template<typename Class, typename EventType>
	void Subscribe(void(Class::*Callback)(const EventType* const))
	{
		IEventDelegate* delegate = new EventDelegate<Class, EventType>(static_cast<Class*>(this), Callback);
		m_eventHandler->Subscribe<EventType>(delegate);
	}
	template<typename Class, typename EventType>
	void Unsubscribe(void(Class::*Callback)(const EventType* const))
	{
		EventDelegate<Class, EventType> delegate(static_cast<Class*>(this), Callback);
		m_eventHandler->Unsubscribe<EventType>(delegate);
	}

private:
	EventHandler* m_eventHandler;
	bool m_enabled;

};
