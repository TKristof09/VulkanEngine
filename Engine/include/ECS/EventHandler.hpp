#pragma once

#include "ECS/Types.hpp"
#include <unordered_map>
#include <list>
#include <memory>
#include <vector>

#include "ECS/IEventDelegate.hpp"
#include "Memory/LinearAllocator.hpp"

#define EVENT_BUFFER_SIZE 4194304 // 4 MB
class EventHandler
{
public:
	EventHandler();
	~EventHandler();

	void DispatchEvents();


	template<typename T, typename... Args>
	void Send(Args... args)
	{
		void* memory = m_allocator->Allocate(sizeof(T), alignof(T));
		if(memory != nullptr)
			m_pendingEvents.push_back(new(memory)T(std::forward<Args>(args)...));
		else
			std::cerr << "Event buffer is full" << std::endl;
	}


	
	template<typename Class, typename EventType>
	void Subscribe(Class* owner, void(Class::* Callback)(const EventType* const))
	{
		IEventDelegate* delegate = new EventDelegate<Class, EventType>(owner, Callback);
		
		m_eventCallbacks[EventType::STATIC_EVENT_TYPE_ID].push_back(std::unique_ptr<IEventDelegate>(delegate));

	}
	template<typename Class, typename EventType>
	void Unsubscribe(Class* owner, void(Class::* Callback)(const EventType* const))
	{
		EventDelegate<Class, EventType> delegate(owner, Callback);

		auto it = m_eventCallbacks.find(EventType::STATIC_EVENT_TYPE_ID);
		if(it != m_eventCallbacks.end())
			it->second.remove_if([&](IEventDelegate* other) { return other->operator==(delegate); });
	}
private:

	LinearAllocator* m_allocator;
	std::unordered_map<EventTypeID, std::list<std::unique_ptr<IEventDelegate>>> m_eventCallbacks;
	std::vector<IEvent*> m_pendingEvents;
};
