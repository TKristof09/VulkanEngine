#pragma once

#include "ECS/EventHandler.hpp"
#include "ECS/Types.hpp"
#include "ECS/EventDelegate.hpp"
#include "ECS/ECSEngine.hpp"

class ISystem
{
	friend class SystemManager;
	friend class EventHandler;
public:
	virtual ~ISystem() {};

	virtual void PreUpdate(double dt) = 0;
	virtual void Update(double dt) = 0;
	virtual void PostUpdate(double dt) = 0;
	virtual void FixedUpdate(double dt) = 0;

	virtual const SystemTypeID GetTypeID() const = 0;

	inline bool IsEnabled() const { return m_enabled; }

protected:
	template<typename EventType, typename... Args>
	void SendEvent(Args... args)
	{
		m_ecsEngine->eventHandler->Send<EventType>(std::forward<Args>(args)...);
	}

	template<typename Class, typename EventType>
	void Subscribe(void(Class::*Callback)(const EventType* const))
	{
		m_ecsEngine->eventHandler->Subscribe(static_cast<Class*>(this), Callback);
	}
	template<typename Class, typename EventType>
	void Unsubscribe(void(Class::*Callback)(const EventType* const))
	{
		m_ecsEngine->eventHandler->Unsubscribe(static_cast<Class*>(this), Callback);
	}

	ECSEngine* m_ecsEngine;

private:
	bool m_enabled;

};
