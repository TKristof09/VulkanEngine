#pragma once
#include "ECS/CoreSystems/RendererSystem.hpp"
#include "ECS/Entity.hpp"
#include "ECS/CoreEvents/EntityEvents.hpp"
#include "Utils/DebugUIElements.hpp"

class HierarchyUI
{
public:
	HierarchyUI(RendererSystem* rendererSystem);


	void OnEntityCreated(const EntityCreated* event);

private:
	void EntitySelectedCallback(EntityID entity);
	
	void SetupComponentProperties(ComponentTypeID type, IComponent* component);
	RendererSystem* m_rendererSystem;

	std::unordered_map<EntityID, std::shared_ptr<TreeNode>> m_hierarchyTree;
	DebugUIWindow m_hierarchyWindow;
	DebugUIWindow m_propertiesWindow;

	EntityID m_selectedEntity;
};

