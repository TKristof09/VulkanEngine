#include "ECS/Core.hpp"


ECS::ECS()
{
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
    return {m_world, name};
}

Entity ECS::CreateChildEntity(Entity* parent, const std::string& name)
{
    return {m_world, name, parent};
}

void ECS::DestroyEntity(Entity& entity)
{
    entity.m_entity.destruct();
}
