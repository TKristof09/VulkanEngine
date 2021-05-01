#pragma once
#include "Rendering/Renderer.hpp"
#include "ECS/Entity.hpp"
#include "ECS/CoreEvents/EntityEvents.hpp"
#include "Utils/DebugUIElements.hpp"

typedef std::function<void(IComponent*, DebugUIWindow*)> PropertyDrawFunction; //void (*PropertyDrawFunction)(IComponent*, DebugUIWindow*);

class HierarchyUI
{
public:
	HierarchyUI(ECSEngine* ecs, Renderer* renderer, MaterialSystem* materialSystem);


	void OnEntityCreated(const EntityCreated* event);

	static void RegisterPropertyDrawFunction(ComponentTypeID typeID, PropertyDrawFunction func)
	{
		m_propertyDrawFunctions[typeID] = func;
	}

private:
	void EntitySelectedCallback(EntityID entity);

	Renderer* m_renderer;
	ECSEngine* m_ecs;

	std::unordered_map<EntityID, std::shared_ptr<TreeNode>> m_hierarchyTree;
	DebugUIWindow m_hierarchyWindow;
	DebugUIWindow m_propertiesWindow;

	EntityID m_selectedEntity;

	static std::unordered_map<ComponentTypeID, PropertyDrawFunction> m_propertyDrawFunctions;

};

