#pragma once

#include <typeindex>
#include <unordered_map>
#include <list>
#include <memory>
#include <vector>

#include "Core/Events/EventDelegate.hpp"
#include "Core/Events/Event.hpp"
#include "ECS/CoreEvents/ComponentEvents.hpp"
#include "Memory/LinearAllocator.hpp"
#include "Application.hpp"
#include "ECS/Core.hpp"
#include "ECS/Observer.hpp"

#define EVENT_BUFFER_SIZE 4194304  // 4 MB
class EventHandler
{
public:
    EventHandler();
    ~EventHandler();

    void DispatchEvents();


    template<typename T, typename... Args>
    void Send(Args&&... args)
    {
        void* memory = m_allocator->Allocate(sizeof(T), alignof(T));
        if(memory != nullptr)
            m_pendingEvents.push_back(new(memory) T(std::forward<Args>(args)...));
        else
            LOG_ERROR("Event buffer is full");
    }
    template<typename T>
    void Send(T e)
    {
        void* memory = m_allocator->Allocate(sizeof(T), alignof(T));
        if(memory != nullptr)
            m_pendingEvents.push_back(new(memory) T(e));
        else
            LOG_ERROR("Event buffer is full");
    }


    template<typename Class, typename EventType>
    void Subscribe(Class* owner, void (Class::*Callback)(EventType))
    {
        m_eventCallbacks[typeid(EventType)].emplace_back(std::make_unique<EventDelegate<EventType>>(std::bind(Callback, owner, std::placeholders::_1)));
    }
    template<typename EventType>
    void Subscribe(std::function<void(EventType)> Callback)
    {
        m_eventCallbacks[typeid(EventType)].emplace_back(std::make_unique<EventDelegate<EventType>>(Callback));
    }

    // specific overloads for component events because we need to register them with the ECS when they are subscribed to for the first time
    template<typename Class, typename ComponentType>
    void Subscribe(Class* owner, void (Class::*Callback)(ComponentAdded<ComponentType>));
    template<typename ComponentType>
    void Subscribe(std::function<void(ComponentAdded<ComponentType>)> Callback);
    template<typename Class, typename ComponentType>
    void Subscribe(Class* owner, void (Class::*Callback)(ComponentRemoved<ComponentType>));
    template<typename ComponentType>
    void Subscribe(std::function<void(ComponentRemoved<ComponentType>)> Callback);


    void OnSceneSwitched(SceneSwitchedEvent e);

private:
    std::unique_ptr<LinearAllocator> m_allocator;
    std::unordered_map<std::type_index, std::list<std::unique_ptr<IEventDelegate>>> m_eventCallbacks;
    std::vector<Event*> m_pendingEvents;  // these pointers will get deleted in the destructor of the linear allocator
};


template<typename Class, typename ComponentType>
void EventHandler::Subscribe(Class* owner, void (Class::*Callback)(ComponentAdded<ComponentType>))
{
    auto it = m_eventCallbacks.find(typeid(ComponentAdded<ComponentType>));
    if(it == m_eventCallbacks.end())
    {
        ECS* ecs = Application::GetInstance()->GetScene()->GetECS();
        ecs->AddObserver<ComponentType>(ECSEvent::OnAdd,
                                        [this](flecs::entity e, ComponentType& /*component*/)
                                        {
                                            ComponentAdded<ComponentType> event;
                                            event.entity = Entity(e);
                                            Send<ComponentAdded<ComponentType>>(event);
                                        });

        // need to reregister with the new ecs when the scene is switched
        Subscribe<SceneSwitchedEvent>(
            [this](SceneSwitchedEvent e)
            {
                e.newScene->ecs->AddObserver<ComponentType>(ECSEvent::OnAdd,
                                                            [this](flecs::entity e, ComponentType& /*component*/)
                                                            {
                                                                ComponentAdded<ComponentType> event;
                                                                event.entity = Entity(e);
                                                                Send<ComponentAdded<ComponentType>>(event);
                                                            });
            });
    }
    m_eventCallbacks[typeid(ComponentAdded<ComponentType>)].emplace_back(std::make_unique<EventDelegate<ComponentAdded<ComponentType>>>(std::bind(Callback, owner, std::placeholders::_1)));
}

template<typename ComponentType>
void EventHandler::Subscribe(std::function<void(ComponentAdded<ComponentType>)> Callback)
{
    auto it = m_eventCallbacks.find(typeid(ComponentAdded<ComponentType>));
    if(it == m_eventCallbacks.end())
    {
        ECS* ecs = Application::GetInstance()->GetScene()->GetECS();
        ecs->AddObserver<ComponentType>(ECSEvent::OnAdd,
                                        [this](flecs::entity e, ComponentType& component)
                                        {
                                            ComponentAdded<ComponentType> event;
                                            event.entity = Entity(e);
                                            Send<ComponentAdded<ComponentType>>(event);
                                        });
        // need to reregister with the new ecs when the scene is switched
        Subscribe<SceneSwitchedEvent>(
            [this](SceneSwitchedEvent e)
            {
                e.newScene->ecs->AddObserver<ComponentType>(ECSEvent::OnAdd,
                                                            [this](flecs::entity e, ComponentType& component)
                                                            {
                                                                ComponentAdded<ComponentType> event;
                                                                event.entity = Entity(e);
                                                                Send<ComponentAdded<ComponentType>>(event);
                                                            });
            });
    }
    m_eventCallbacks[typeid(ComponentAdded<ComponentType>)].emplace_back(std::make_unique<EventDelegate<ComponentAdded<ComponentType>>>(Callback));
}

template<typename Class, typename ComponentType>
void EventHandler::Subscribe(Class* owner, void (Class::*Callback)(ComponentRemoved<ComponentType>))
{
    auto it = m_eventCallbacks.find(typeid(ComponentRemoved<ComponentType>));
    if(it == m_eventCallbacks.end())
    {
        ECS* ecs = Application::GetInstance()->GetScene()->GetECS();
        ecs->AddObserver<ComponentType>(ECSEvent::OnRemove,
                                        [this](flecs::entity e, ComponentType& /*component*/)
                                        {
                                            ComponentRemoved<ComponentType> event;
                                            event.entity = Entity(e);
                                            Send<ComponentRemoved<ComponentType>>(event);
                                        });

        // need to reregister with the new ecs when the scene is switched
        Subscribe<SceneSwitchedEvent>(
            [this](SceneSwitchedEvent e)
            {
                e.newScene->ecs->AddObserver<ComponentType>(ECSEvent::OnRemove,
                                                            [this](flecs::entity e, ComponentType& /*component*/)
                                                            {
                                                                ComponentRemoved<ComponentType> event;
                                                                event.entity = Entity(e);
                                                                Send<ComponentRemoved<ComponentType>>(event);
                                                            });
            });
    }
    m_eventCallbacks[typeid(ComponentRemoved<ComponentType>)].emplace_back(std::make_unique<EventDelegate<ComponentRemoved<ComponentType>>>(std::bind(Callback, owner, std::placeholders::_1)));
}

template<typename ComponentType>
void EventHandler::Subscribe(std::function<void(ComponentRemoved<ComponentType>)> Callback)
{
    auto it = m_eventCallbacks.find(typeid(ComponentRemoved<ComponentType>));
    if(it == m_eventCallbacks.end())
    {
        ECS* ecs = Application::GetInstance()->GetScene()->GetECS();
        ecs->AddObserver<ComponentType>(ECSEvent::OnRemove,
                                        [this](flecs::entity e, ComponentType& component)
                                        {
                                            ComponentRemoved<ComponentType> event;
                                            event.entity = Entity(e);
                                            Send<ComponentRemoved<ComponentType>>(event);
                                        });
        // need to reregister with the new ecs when the scene is switched
        Subscribe<SceneSwitchedEvent>(
            [this](SceneSwitchedEvent e)
            {
                e.newScene->ecs->AddObserver<ComponentType>(ECSEvent::OnRemove,
                                                            [this](flecs::entity e, ComponentType& component)
                                                            {
                                                                ComponentRemoved<ComponentType> event;
                                                                event.entity = Entity(e);
                                                                Send<ComponentRemoved<ComponentType>>(event);
                                                            });
            });
    }
    m_eventCallbacks[typeid(ComponentRemoved<ComponentType>)].emplace_back(std::make_unique<EventDelegate<ComponentRemoved<ComponentType>>>(Callback));
}
