#pragma once

#include "flecs.h"
#include "ECS/Entity.hpp"

class ECS
{
public:
    ECS();
    ~ECS();

    void Update(float dt);

    Entity CreateEntity(const std::string& name = "Entity");
    Entity CreateChildEntity(Entity* parent, const std::string& name = "Entity");
    void DestroyEntity(Entity& entity);

    template<typename T, typename... Args>
    void AddSystem(Args&&... args);
    // Do we want to be able to remove systems?

    template<typename... Components>
    flecs::query<Components...> StartQueryBuilder()
    {
        return m_world.query_builder<Components...>();
    }

    void EnableRESTEditor()
    {
        m_world.set<flecs::Rest>({});
        // clang-format off
        m_world.import<flecs::monitor>(); // clang formats this weirdly because if the import keyword
        // clang-format on
    }

private:
    flecs::world m_world;
};

template<typename T, typename... Args>
void ECS::AddSystem(Args&&... args)
{
    T system(std::forward<Args>(args)...);
    system.m_world = &m_world;
    system.Initialize();
    // we just need to initialise it so that the system will register itself with the world and we don't have to keep it around after as systems should have no persistent state
}
