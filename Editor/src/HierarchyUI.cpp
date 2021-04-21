#include "HierarchyUI.hpp"
#include "ECS/CoreComponents/Relationship.hpp"
#include "ECS/CoreComponents/NameTag.hpp"
#include "ECS/EntityManager.hpp"


HierarchyUI::HierarchyUI(RendererSystem* rendererSystem):
	m_hierarchyWindow(DebugUIWindow("Hierarchy")),
	m_propertiesWindow(DebugUIWindow("Properties"))
{
	rendererSystem->AddDebugUIWindow(&m_hierarchyWindow);
	rendererSystem->AddDebugUIWindow(&m_propertiesWindow);

	ECSEngine::GetInstance().eventHandler->Subscribe(this, &HierarchyUI::OnEntityCreated);
}

void HierarchyUI::OnEntityCreated(const EntityCreated* event)
{
	Relationship* comp = event->entity.GetComponent<Relationship>();

	auto node = std::make_shared<TreeNode>(event->entity.GetComponent<NameTag>()->name);
	node->SetUserPtr(event->entity.GetEntityID());
	node->RegisterCallback(
			[this](TreeNode* node) 
			{
				EntitySelectedCallback((EntityID)node->GetUserPtr());
				m_hierarchyWindow.DeselectAll();
				node->SetIsSelected(true);

			}
	);

	if(comp->parent == INVALID_ENTITY_ID)
	{
		m_hierarchyWindow.AddElement(node);
		m_hierarchyTree[event->entity.GetEntityID()] = node;
	}
	else
	{
		
		m_hierarchyTree[comp->parent]->AddElement(node);
		m_hierarchyTree[event->entity.GetEntityID()] = node;
	}
}

void HierarchyUI::EntitySelectedCallback(EntityID entity)
{
	m_selectedEntity = entity;
	LOG_TRACE("Selected entity: {0}", ECSEngine::GetInstance().componentManager->GetComponent<NameTag>(m_selectedEntity)->name);

	m_propertiesWindow.Clear();
	auto components = ECSEngine::GetInstance().componentManager->GetAllComponents(entity);

	for(auto& [typeID, component] : components)
	{
		SetupComponentProperties(typeID, component);
		m_propertiesWindow.AddElement(std::make_shared<Separator>());
	}
}

void HierarchyUI::SetupComponentProperties(ComponentTypeID type, IComponent* component)
{
	if(type == Transform::STATIC_COMPONENT_TYPE_ID) 
	{
		Transform* comp = (Transform*)component;
		auto position = std::make_shared<DragVector3>(&comp->lPosition, "Position");
		auto scale = std::make_shared<DragVector3>(&comp->lScale, "Scale");

		m_propertiesWindow.AddElement(position);
		m_propertiesWindow.AddElement(scale);
		
		return;
	}
	if(type == NameTag::STATIC_COMPONENT_TYPE_ID)
	{
		auto name = std::make_shared<Text>(((NameTag*)component)->name);
		m_propertiesWindow.AddElement(name);

		return;
	}

	if(type == Material::STATIC_COMPONENT_TYPE_ID)
	{
		Material* comp = (Material*)component;

		auto shader = std::make_shared<Text>(comp->shaderName);
		m_propertiesWindow.AddElement(shader);

		auto table = std::make_shared<Table>();
		int row = 0;
		for(auto& [name, path] : comp->textures)
		{
			auto nameText = std::make_shared<Text>(name);
			auto pathText = std::make_shared<Text>(path);

			table->AddElement(nameText, row, 1);
			table->AddElement(pathText, row, 2);
			row++;
		}
		m_propertiesWindow.AddElement(table);
	}
}