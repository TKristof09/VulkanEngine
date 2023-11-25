#include "ECS/Core.hpp"
#include "ECS/CoreSystems/TransformSystem.hpp"


ECS::ECS()
{
    AddSystem<TransformSystem>();
}

ECS::~ECS()
{
}

void ECS::Update(float dt)
{
    m_world.progress(dt);
}

Entity ECS::CreateEntity(const std::string& name)
{
    Entity e{m_world, name};
    return e;
}

Entity ECS::CreateChildEntity(Entity* parent, const std::string& name)
{
    Entity e{m_world, name, parent};
    e.AddComponent<Transform>();
    return e;
}

void ECS::DestroyEntity(Entity& entity)
{
    entity.m_entity.destruct();
}
