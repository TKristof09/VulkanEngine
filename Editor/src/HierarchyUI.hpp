#pragma once
#include <utility>

#include "Rendering/Renderer.hpp"
#include "ECS/Entity.hpp"
#include "ECS/CoreEvents/EntityEvents.hpp"
#include "Utils/DebugUIElements.hpp"

class MaterialSystem;
using PropertyDrawFunction = std::function<void(void*, DebugUIWindow*)>;  // void (*PropertyDrawFunction)(IComponent*, DebugUIWindow*);

class HierarchyUI
{
public:
    HierarchyUI(Scene* scene, Renderer* renderer, MaterialSystem* materialSystem);


    void OnEntityCreated(EntityCreated event);

    template<typename T>
    static void RegisterPropertyDrawFunction(PropertyDrawFunction func)
    {
        m_propertyDrawFunctions[typeid(T)] = std::move(func);
    }

private:
    void EntitySelectedCallback(Entity entity);

    template<typename T>
    void DrawComponent(Entity entity)
    {
        if(!entity.HasComponent<T>())
            return;

        m_propertiesWindow.AddElement(std::make_shared<Separator>());
        m_propertyDrawFunctions[typeid(T)](entity.GetComponentMut<T>(), &m_propertiesWindow);
        m_propertiesWindow.AddElement(std::make_shared<Separator>());
    }

    Renderer* m_renderer;

    std::unordered_map<Entity, std::shared_ptr<TreeNode>> m_hierarchyTree;
    DebugUIWindow m_hierarchyWindow;
    DebugUIWindow m_propertiesWindow;

    Entity m_selectedEntity = Entity::INVALID_ENTITY;

    static std::unordered_map<std::type_index, PropertyDrawFunction> m_propertyDrawFunctions;
};
