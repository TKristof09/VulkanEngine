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

	RendererSystem* m_rendererSystem;

	std::unordered_map<EntityID, TreeNode*> m_hierarchyTree;
	DebugUIWindow m_window;

	EntityID m_selectedEntity;
};

