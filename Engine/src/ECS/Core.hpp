#pragma once

#include "flecs.h"
#include "ECS/Entity.hpp"
#include "ECS/Query.hpp"
#include "ECS/Observer.hpp"


class ECS
{
public:
    ECS();
    ~ECS();

    void Update(float dt);


    Entity CreateEntity(const std::string& name = "Entity");
    Entity CreateChildEntity(const Entity* parent, const std::string& name = "Entity");
    void DestroyEntity(Entity& entity);

    template<typename T, typename... Args>
    void AddSystem(Args&&... args);
    // Do we want to be able to remove systems?

    template<typename T>
    const T* GetSingleton()
    {
        return m_world.get<T>();
    }

    template<typename T>
    T* GetSingletonMut()
    {
        return m_world.get_mut<T>();
    }

    template<typename T>
    void AddSingleton()
    {
        m_world.add<T>();
    }
    template<typename T>
    void SetSingleton(const T& singleton)
    {
        m_world.set<T>(singleton);
    }

    template<typename T, typename... Args>
    void EmplaceSingleton(Args&&... args)
    {
        m_world.emplace<T>(std::forward<Args>(args)...);
    }


    template<typename... Components>
    QueryBuilder<Components...> StartQueryBuilder(const std::string& name = "")
    {
        return m_world.query_builder<Components...>(name.empty() ? nullptr : name.c_str());
    }

    template<typename Component>
    Observer AddObserver(ECSEvent eventType, std::function<void(flecs::entity, Component&)> callback)
    {
        switch(eventType)
        {
        case ECSEvent::OnAdd:
            return m_world.observer<Component>().event(flecs::OnAdd).each(callback);
        case ECSEvent::OnSet:
            return m_world.observer<Component>().event(flecs::OnSet).each(callback);
        case ECSEvent::OnRemove:
            return m_world.observer<Component>().event(flecs::OnRemove).each(callback);
        }
    }
    void EnableRESTEditor()
    {
        m_world.set<flecs::Rest>({});
        // clang-format off
        m_world.import<flecs::monitor>(); // clang formats this weirdly because if the import keyword
                                           // clang-format on
    }

private:
    friend class Entity;
    flecs::world m_world;
    std::unordered_map<std::string, uint32_t> m_entityNames;
};

template<typename T, typename... Args>
void ECS::AddSystem(Args&&... args)
{
    T system(std::forward<Args>(args)...);
    system.m_world = &m_world;
    system.Initialize();
    // we just need to initialise it so that the system will register itself with the world and we don't have to keep it around after as systems should have no persistent state
}
