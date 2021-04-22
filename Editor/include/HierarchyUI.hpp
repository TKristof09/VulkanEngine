#pragma once
#include "ECS/CoreSystems/RendererSystem.hpp"
#include "ECS/Entity.hpp"
#include "ECS/CoreEvents/EntityEvents.hpp"
#include "Utils/DebugUIElements.hpp"

typedef void (*PropertyDrawFunction)(IComponent*, DebugUIWindow*);

class HierarchyUI
{
public:
	HierarchyUI(RendererSystem* rendererSystem);


	void OnEntityCreated(const EntityCreated* event);

	static void RegisterPropertyDrawFunction(ComponentTypeID typeID, PropertyDrawFunction func)
	{
		m_propertyDrawFunctions[typeID] = func;
	}

private:
	void EntitySelectedCallback(EntityID entity);

	RendererSystem* m_rendererSystem;

	std::unordered_map<EntityID, std::shared_ptr<TreeNode>> m_hierarchyTree;
	DebugUIWindow m_hierarchyWindow;
	DebugUIWindow m_propertiesWindow;

	EntityID m_selectedEntity;

	static std::unordered_map<ComponentTypeID, PropertyDrawFunction> m_propertyDrawFunctions;

};

