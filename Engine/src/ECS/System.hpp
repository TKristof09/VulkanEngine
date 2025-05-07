#pragma once

#include <flecs.h>

enum class SystemPhase
{
    OnStart,
    OnLoad,
    PreUpdate,
    OnUpdate,
    PostUpdate,
    PreRender,
};

template<typename... Components>
using SystemBuilder = flecs::system_builder<Components...>;

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
    void AddContextSingleton(T context);

    // contextTerm is indexed from 1
    template<class... Components>
    void Register(SystemPhase phase, void (*fn)(flecs::iter& it, size_t i, Components... args), uint8_t contextTerm = 0);

    // contextTerm is indexed from 1
    template<class... Components>
    void Register(SystemPhase phase, void (*fn)(flecs::entity e, Components... args), uint8_t contextTerm = 0);

    // contextTerm is indexed from 1
    template<class... Components>
    void Register(SystemPhase phase, void (*fn)(Components... args), uint8_t contextTerm = 0);

    // contextTerm is indexed from 1
    template<class... Components>
    void RegisterFixed(SystemPhase phase, void (*fn)(flecs::iter& it, size_t i, Components... args), uint32_t framerate, uint8_t contextTerm = 0);

    // contextTerm is indexed from 1
    template<class... Components>
    void RegisterFixed(SystemPhase phase, void (*fn)(flecs::entity e, Components... args), uint32_t framerate, uint8_t contextTerm = 0);

    // contextTerm is indexed from 1
    template<class... Components>
    void RegisterFixed(SystemPhase phase, void (*fn)(Components... args), uint32_t framerate, uint8_t contextTerm = 0);

    // expose this for systems that want to do more advanced queries, such as traversing the entity hierarchy,etc...
    // we put the function as a parameter to be able to deduce the template parameters so we dont have to specify them one by one when calling this function
    template<class... Components>
    void StartSystemBuilder(SystemPhase phase, void (*buildFn)(SystemBuilder<Components...>& builder));

    // after: Fn can be a generic lambda, captureless or not
    template<class... Components, typename Fn>
    void StartSystemBuilder(SystemPhase phase, Fn buildFn);

private:
    virtual const char* GetName() const { return nullptr; };

    flecs::world* m_world = nullptr;
    friend class ECS;
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
        default:
            LOG_ERROR("Unknown system phase");
            assert(false);
            return flecs::OnStart;
        }
    }
};

template<typename T>
void System::AddContextSingleton(T context)
{
    m_world->set<T>(context);
}

// contextTerm is indexed from 1
template<class... Components>
void System::Register(SystemPhase phase, void (*fn)(flecs::iter& it, size_t i, Components... args), uint8_t contextTerm)
{
    assert(contextTerm <= sizeof...(Components));

    auto builder = m_world->system<Components...>(GetName())
                       .kind(ConvertPhase(phase));
    if(contextTerm > 0)
        builder = builder.term_at(contextTerm).singleton();  // +1 because term_at isn't zero indexed
    builder.each(fn);
}

// contextTerm is indexed from 1
template<class... Components>
void System::Register(SystemPhase phase, void (*fn)(flecs::entity e, Components... args), uint8_t contextTerm)
{
    assert(contextTerm <= sizeof...(Components));

    auto builder = m_world->system<Components...>(GetName())
                       .kind(ConvertPhase(phase));
    if(contextTerm > 0)
        builder = builder.term_at(contextTerm).singleton();  // +1 because term_at isn't zero indexed

    builder.each(fn);
}

// contextTerm is indexed from 1
template<class... Components>
void System::Register(SystemPhase phase, void (*fn)(Components... args), uint8_t contextTerm)
{
    assert(contextTerm <= sizeof...(Components));

    auto builder = m_world->system<Components...>(GetName())
                       .kind(ConvertPhase(phase));
    if(contextTerm > 0)
        builder = builder.term_at(contextTerm).singleton();
    builder.each(fn);
}


// contextTerm is indexed from 1
template<class... Components>
void System::RegisterFixed(SystemPhase phase, void (*fn)(flecs::iter& it, size_t i, Components... args), uint32_t framerate, uint8_t contextTerm)
{
    assert(contextTerm <= sizeof...(Components));

    auto builder = m_world->system<Components...>(GetName())
                       .kind(ConvertPhase(phase))
                       .interval(1.0f / framerate);
    if(contextTerm > 0)
        builder = builder.term_at(contextTerm).singleton();  // +1 because term_at isn't zero indexed
    builder.each(fn);
}

// contextTerm is indexed from 1
template<class... Components>
void System::RegisterFixed(SystemPhase phase, void (*fn)(flecs::entity e, Components... args), uint32_t framerate, uint8_t contextTerm)
{
    assert(contextTerm <= sizeof...(Components));

    auto builder = m_world->system<Components...>(GetName())
                       .kind(ConvertPhase(phase))
                       .interval(1.0f / framerate);
    if(contextTerm > 0)
        builder = builder.term_at(contextTerm).singleton();  // +1 because term_at isn't zero indexed

    builder.each(fn);
}

// contextTerm is indexed from 1
template<class... Components>
void System::RegisterFixed(SystemPhase phase, void (*fn)(Components... args), uint32_t framerate, uint8_t contextTerm)
{
    assert(contextTerm <= sizeof...(Components));

    auto builder = m_world->system<Components...>(GetName())
                       .kind(ConvertPhase(phase))
                       .interval(1.0f / framerate);
    if(contextTerm > 0)
        builder = builder.term_at(contextTerm).singleton();
    builder.each(fn);
}

// expose this for systems that want to do more advanced queries, such as traversing the entity hierarchy,etc...
// we put the function as a parameter to be able to deduce the template parameters so we dont have to specify them one by one when calling this function
template<class... Components>
void System::StartSystemBuilder(SystemPhase phase, void (*buildFn)(SystemBuilder<Components...>& builder))
{
    buildFn(m_world->system<Components...>(GetName()).kind(ConvertPhase(phase)));
}
template<class... Components, typename Fn>
void System::StartSystemBuilder(SystemPhase phase, Fn buildFn)
{
    static_assert(std::is_invocable_v<Fn, SystemBuilder<Components...>&>, "buildFn must be invocable with SystemBuilder<Components…>&");
    buildFn(m_world->system<Components...>(GetName()).kind(ConvertPhase(phase)));
}
