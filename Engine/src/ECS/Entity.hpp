#pragma once
#include "flecs.h"

class ECS;

using EntityIterFn = void(flecs::entity);

struct Static
{
};
class Entity
{
public:
    // Allow constructing a wrapper around a flecs entity
    Entity(flecs::entity entity)
        : m_entity(entity) {}

    template<typename T>
    void AddComponent();

    template<typename T>
    void AddComponent(T& component);

    template<typename T, typename... Args>
    void AddComponent(Args&&... args);

    template<typename T>
    const T* GetComponent() const;

    template<typename T>
    T* GetComponentMut();

    template<typename T>
    [[nodiscard]] bool HasComponent() const;

    template<typename T>
    void RemoveComponent();

    Entity CreateChild(const std::string& name = "Entity");

    void IterateChildren(EntityIterFn f);

    void SetStatic(bool isStatic);


private:
    friend class ECS;
    Entity(const flecs::world& world, const std::string& name, Entity* parent = nullptr);

    flecs::entity m_entity;
};

inline Entity::Entity(const flecs::world& world, const std::string& name, Entity* parent)
{
    if(parent)
    {
        m_entity = world.entity(name.c_str()).child_of(parent->m_entity);
    }
    else
    {
        m_entity = world.entity(name.c_str());
    }
}

template<typename T>
void Entity::AddComponent()
{
    m_entity.add<T>();
}

template<typename T>
void Entity::AddComponent(T& component)
{
    if constexpr(std::is_default_constructible<T>::value)
    {
        m_entity.set<T>(component);
    }
    else
    {
        m_entity.emplace<T>(component);
    }
}

template<typename T, typename... Args>
void Entity::AddComponent(Args&&... args)
{
    if constexpr(std::is_default_constructible<T>::value)
    {
        m_entity.set<T>(std::forward<Args>(args)...);
    }
    else
    {
        m_entity.emplace<T>(std::forward<Args>(args)...);
    }
}

template<typename T>
const T* Entity::GetComponent() const
{
    return m_entity.get<T>();
}

template<typename T>
T* Entity::GetComponentMut()
{
    if(!HasComponent<T>())
    {
        return nullptr;
    }
    return m_entity.get_mut<T>();
}

template<typename T>
bool Entity::HasComponent() const
{
    return m_entity.has<T>();
}

template<typename T>
void Entity::RemoveComponent()
{
    m_entity.remove<T>();
}

inline Entity Entity::CreateChild(const std::string& name)
{
    return {m_entity.world(), name, this};
}

inline void Entity::IterateChildren(EntityIterFn f)
{
    m_entity.children(f);
}

inline void Entity::SetStatic(bool isStatic)
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
