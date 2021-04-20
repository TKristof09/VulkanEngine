#include "HierarchyUI.hpp"
#include "ECS/CoreComponents/Relationship.hpp"
#include "ECS/CoreComponents/NameTag.hpp"
#include "ECS/EntityManager.hpp"


HierarchyUI::HierarchyUI(RendererSystem* rendererSystem):
	m_window(DebugUIWindow("Hierarchy"))
{
	rendererSystem->AddDebugUIWindow(&m_window);
	ECSEngine::GetInstance().eventHandler->Subscribe(this, &HierarchyUI::OnEntityCreated);
}

void HierarchyUI::OnEntityCreated(const EntityCreated* event)
{
	Relationship* comp = event->entity.GetComponent<Relationship>();

	auto node = new TreeNode(event->entity.GetComponent<NameTag>()->name);
	node->SetUserPtr(event->entity.GetEntityID());
	node->RegisterCallback(
			[this](TreeNode* node) 
			{
				m_selectedEntity = (EntityID)node->GetUserPtr();
				m_window.DeselectAll();
				node->SetIsSelected(true);
				LOG_TRACE("Selected entity: {0}", ECSEngine::GetInstance().componentManager->GetComponent<NameTag>(m_selectedEntity)->name);
			}
	);

	if(comp->parent == INVALID_ENTITY_ID)
	{
		m_window.AddElement(node);
		m_hierarchyTree[event->entity.GetEntityID()] = node;
	}
	else
	{
		
		m_hierarchyTree[comp->parent]->AddElement(node);
		m_hierarchyTree[event->entity.GetEntityID()] = node;
	}
}
