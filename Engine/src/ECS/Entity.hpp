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
    Entity()
        : m_entity(flecs::entity::null()) {}
    // Allow constructing a wrapper around a flecs entity
    Entity(flecs::entity entity)
        : m_entity(entity) {}

    Entity(const Entity& other)            = default;
    Entity& operator=(const Entity& other) = default;

    Entity(Entity&& other) noexcept
        : m_entity(other.m_entity)
    {
        other.m_entity = flecs::entity::null();
    }
    Entity& operator=(Entity&& other) noexcept
    {
        m_entity       = other.m_entity;
        other.m_entity = flecs::entity::null();
        return *this;
    }


    template<typename T>
    void AddComponent();

    template<typename T>
    void SetComponent(const T& component);
    template<typename T>
    void SetComponent(T&& component);


    template<typename T, typename... Args>
    void EmplaceComponent(Args&&... args);

    template<typename T>
    const T* GetComponent() const;

    template<typename T>
    T* GetComponentMut();

    template<typename T>
    [[nodiscard]] bool HasComponent() const;

    template<typename T>
    void RemoveComponent();

    [[nodiscard]] Entity CreateChild(const std::string& name = "Entity") const;

    void IterateChildren(EntityIterFn f);

    void SetStatic(bool isStatic);

    [[nodiscard]] std::string GetName() const { return m_entity.name().c_str(); }

    [[nodiscard]] Entity GetParent() const
    {
        return {m_entity.parent()};
    }

    bool operator==(const Entity& other) const
    {
        return m_entity == other.m_entity;
    }


    static const Entity INVALID_ENTITY;

    void Print() const
    {
        LOG_INFO("Entity {0} : {1}", m_entity.name().c_str(), m_entity.type().str().c_str());
    }

private:
    friend class ECS;
    friend struct std::hash<Entity>;
    Entity(ECS* ecs, const std::string& name, const Entity* parent = nullptr);

    flecs::entity m_entity;
    ECS* m_ecs = nullptr;
};

inline const Entity Entity::INVALID_ENTITY = Entity{flecs::entity::null()};


// implement std hash for entity by using hash on its member m_entity.id()
namespace std
{
template<>
struct hash<Entity>
{
    std::size_t operator()(const Entity& k) const
    {
        return std::hash<flecs::entity_t>()(k.m_entity.id());
    }
};
}  // namespace std


template<typename T>
void Entity::AddComponent()
{
    m_entity.add<T>();
}

template<typename T>
void Entity::SetComponent(const T& component)
{
    m_entity.set<T>(component);
}

template<typename T>
void Entity::SetComponent(T&& component)
{
    m_entity.set<T>(component);
}

template<typename T, typename... Args>
void Entity::EmplaceComponent(Args&&... args)
{
    m_entity.emplace<T>(std::forward<Args>(args)...);
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
