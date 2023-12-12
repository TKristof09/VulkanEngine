#pragma once

#include "Core/Events/Event.hpp"

class IEventDelegate
{
public:
    virtual ~IEventDelegate(){};
    virtual void Invoke(Event* e) = 0;
};

template<typename EventType>
class EventDelegate : public IEventDelegate
{
public:
    EventDelegate(std::function<void(EventType)> callback)
        : m_callback(callback) {}

    void Invoke(Event* e) override
    {
        (m_callback)(*static_cast<EventType*>(e));
    }

private:
    std::function<void(EventType)> m_callback;
};
