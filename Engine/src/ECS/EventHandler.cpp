#include "ECS/EventHandler.hpp"
#include "ECS/ECSMemoryManager.hpp"


EventHandler::EventHandler()
{
	m_allocator = new LinearAllocator(EVENT_BUFFER_SIZE, ECSMemoryManager::GetInstance().Allocate(EVENT_BUFFER_SIZE));
}

EventHandler::~EventHandler()
{
	m_eventCallbacks.clear();
	m_pendingEvents.clear();

	ECSMemoryManager::GetInstance().Free((void*)m_allocator->GetMemoryStartAddress());

	delete m_allocator;
}

void EventHandler::DispatchEvents()
{
	size_t eventIndex = 0;
	size_t eventEnd = m_pendingEvents.size();
	while(eventIndex < eventEnd) // do it like this to handle if a new event is added during another is handled
	{
		auto iter = m_eventCallbacks.find(m_pendingEvents[eventIndex]->GetEventTypeID());
		if(iter != m_eventCallbacks.end())
		{
			auto& delegates = iter->second;

			for(auto it = delegates.begin(); it != delegates.end(); ++it) // if an element is push_backed to a list the iterator doesn't get invalidated so this works
				(*it)->Invoke(m_pendingEvents[eventIndex]);

			m_pendingEvents[eventIndex]->~IEvent(); // remove if not needed, idk yet whether events need to be destructed or if they can just "magically" disappear
		}
		eventIndex++;
		eventEnd = m_pendingEvents.size(); // update last index in case a new event was added
	}

	m_pendingEvents.clear();
	m_allocator->Clear();

}
