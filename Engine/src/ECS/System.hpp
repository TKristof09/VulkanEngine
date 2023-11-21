#pragma once

#include "flecs.h"

enum class SystemPhase
{
    OnStart,
    OnLoad,
    PreUpdate,
    OnUpdate,
    PostUpdate,
    PreRender,
};
class System
{
public:
    virtual ~System() = default;

protected:
    // The system should register its functionality inside this function
    virtual void Initialize() = 0;

    // Add a singleton component to the ecs that serves as a context for this system
    // This can be retrived by capturing the component in the system's registered function and specifying which term of the function is the context
    template<typename T>
    void AddContextSingleton(T& context)
    {
        m_world->set<T>(context);
    }

    template<class... Components>
    void Register(SystemPhase phase, void (*fn)(flecs::iter& it, size_t i, Components&... args), int8_t contextTerm = -1)
    {
        assert(contextTerm <= sizeof...(Components));

        auto builder = m_world->system<Components...>()
                           .kind(ConvertPhase(phase));
        if(contextTerm >= 0)
            builder = builder.term_at(contextTerm + 1).singleton();  // +1 because term_at isn't zero indexed
        builder.each(fn);
    }
    template<class... Components>
    void Register(SystemPhase phase, void (*fn)(flecs::entity e, Components&... args), int8_t contextTerm = -1)
    {
        assert(contextTerm <= sizeof...(Components));

        auto builder = m_world->system<Components...>()
                           .kind(ConvertPhase(phase));
        if(contextTerm >= 0)
            builder = builder.term_at(contextTerm + 1).singleton();  // +1 because term_at isn't zero indexed

        builder.each(fn);
    }

    template<class... Components>
    void Register(SystemPhase phase, void (*fn)(Components&... args), int8_t contextTerm = -1)
    {
        assert(contextTerm < sizeof...(Components));

        auto builder = m_world->system<Components...>()
                           .kind(ConvertPhase(phase));
        if(contextTerm >= 0)
            builder = builder.term_at(contextTerm + 1).singleton();  // +1 because term_at isn't zero indexed

        builder.each(fn);
    }


    // expose this for systems that want to do more advanced queries, such as traversing the entity hierarchy,etc...
    template<class... Components>
    flecs::system_builder<Components...> StartSystemBuilder()
    {
        return m_world->system<Components...>();
    }

private:
    friend class ECS;
    flecs::world* m_world = nullptr;
    static flecs::entity_t ConvertPhase(SystemPhase phase)
    {
        switch(phase)
        {
        case SystemPhase::OnStart:
            return flecs::OnStart;
        case SystemPhase::OnLoad:
            return flecs::OnLoad;
        case SystemPhase::PreUpdate:
            return flecs::PreUpdate;
        case SystemPhase::OnUpdate:
            return flecs::OnUpdate;
        case SystemPhase::PostUpdate:
            return flecs::PostUpdate;
        case SystemPhase::PreRender:
            return flecs::PreStore;
        }
    }
};
