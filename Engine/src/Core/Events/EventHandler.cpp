#include "Core/Events/EventHandler.hpp"


EventHandler::EventHandler()
{
    m_allocator = std::make_unique<LinearAllocator>(EVENT_BUFFER_SIZE);
}

EventHandler::~EventHandler()
{
    m_eventCallbacks.clear();
    m_pendingEvents.clear();
}

void EventHandler::DispatchEvents()
{
    size_t eventIndex = 0;
    size_t eventEnd   = m_pendingEvents.size();
    while(eventIndex < eventEnd)  // do it like this to handle if a new event is added during another is handled
    {
        auto iter = m_eventCallbacks.find(m_pendingEvents[eventIndex]->GetEventTypeID());
        if(iter != m_eventCallbacks.end())
        {
            auto& delegates = iter->second;

            for(auto it = delegates.begin(); it != delegates.end(); ++it)  // if an element is push_backed to a list the iterator doesn't get invalidated so this works
                (*it)->Invoke(m_pendingEvents[eventIndex]);
        }
        eventIndex++;
        eventEnd = m_pendingEvents.size();  // update last index in case a new event was added
    }

    m_pendingEvents.clear();
    m_allocator->Clear();
}

