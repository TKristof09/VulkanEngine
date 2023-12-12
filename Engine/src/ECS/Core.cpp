#include "ECS/Core.hpp"
#include "ECS/CoreSystems/TransformSystem.hpp"
#include <format>

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
    uint32_t count = m_entityNames[name]++;
    Entity e{this, count > 0 ? std::format("{}_({})", name, count) : name};
    return e;
}

Entity ECS::CreateChildEntity(const Entity* parent, const std::string& name)
{
    std::string scopedName = std::format("{}::{}", parent->m_entity.name().c_str(), name);
    uint32_t count         = m_entityNames[scopedName]++;
    Entity e{this, count > 0 ? std::format("{}_({})", scopedName, count) : scopedName, parent};
    return e;
}

void ECS::DestroyEntity(Entity& entity)
{
    entity.m_entity.destruct();
}
