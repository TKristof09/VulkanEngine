#include "Entity.hpp"

#include "Application.hpp"
#include "Core/Events/EventHandler.hpp"
#include "ECS/CoreEvents/EntityEvents.hpp"
#include "ECS/CoreComponents/InternalTransform.hpp"
#include "ECS/CoreComponents/Transform.hpp"


Entity::Entity(const flecs::world& world, const std::string& name, const Entity* parent)
{
    if(parent)
    {
        m_entity = world.entity(name.c_str()).child_of(parent->m_entity);
    }
    else
    {
        m_entity = world.entity(name.c_str());
    }
    AddComponent<Transform>();
    AddComponent<InternalTransform>();

    EntityCreated event;
    event.entity = *this;
    Application::GetInstance()->GetEventHandler()->Send<EntityCreated>(event);
}

Entity Entity::CreateChild(const std::string& name) const
{
    Entity e{m_entity.world(), name, this};
    return e;
}

void Entity::IterateChildren(EntityIterFn f)
{
    m_entity.children(f);
}

void Entity::SetStatic(bool isStatic)
{
    if(isStatic)
    {
        m_entity.add<Static>();
    }
    else
    {
        m_entity.remove<Static>();
    }
}
