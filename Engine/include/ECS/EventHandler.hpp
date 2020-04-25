#pragma once

#include "ECS/Types.hpp"
#include <unordered_map>
#include <list>

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

	template<typename T>
	void Subscribe(IEventDelegate* delegate)
	{
		m_eventCallbacks[T::STATIC_EVENT_TYPE_ID].push_back(std::unique_ptr<IEventDelegate>(delegate));
	}

	template<typename T>
	void Unsubscribe(IEventDelegate* delegate)
	{
		auto it = m_eventCallbacks.find(T::STATIC_EVENT_TYPE_ID);
		if(it != m_eventCallbacks.end())
			it->second.remove_if([&](IEventDelegate* other){ return other->operator==(delegate); });
	}

private:
	LinearAllocator* m_allocator;
	std::unordered_map<EventTypeID, std::list<std::unique_ptr<IEventDelegate>>> m_eventCallbacks;
	std::vector<IEvent*> m_pendingEvents;
};
