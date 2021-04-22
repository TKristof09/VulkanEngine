#include "HierarchyUI.hpp"
#include "ECS/CoreComponents/Relationship.hpp"
#include "ECS/CoreComponents/NameTag.hpp"
#include "ECS/EntityManager.hpp"
#include "ECS/CoreSystems/MaterialSystem.hpp"
#include "ECS/SystemManager.hpp"

std::unordered_map<ComponentTypeID, PropertyDrawFunction> HierarchyUI::m_propertyDrawFunctions = {};


HierarchyUI::HierarchyUI(RendererSystem* rendererSystem):
	m_hierarchyWindow(DebugUIWindow("Hierarchy")),
	m_propertiesWindow(DebugUIWindow("Properties"))
{
	rendererSystem->AddDebugUIWindow(&m_hierarchyWindow);
	rendererSystem->AddDebugUIWindow(&m_propertiesWindow);

	ECSEngine::GetInstance().eventHandler->Subscribe(this, &HierarchyUI::OnEntityCreated);

	HierarchyUI::RegisterPropertyDrawFunction(Transform::STATIC_COMPONENT_TYPE_ID, [](IComponent* component, DebugUIWindow* window)
	{
		window->AddElement(std::make_shared<Text>("Transform:"));

		Transform* comp = (Transform*)component;
		auto position = std::make_shared<DragVector3>(&comp->lPosition, "Position");
		auto scale = std::make_shared<DragVector3>(&comp->lScale, "Scale");

		window->AddElement(position);
		window->AddElement(scale);
	});

	HierarchyUI::RegisterPropertyDrawFunction(NameTag::STATIC_COMPONENT_TYPE_ID, [](IComponent* component, DebugUIWindow* window)
	{
		auto name = std::make_shared<TextEdit>(&((NameTag*)component)->name);
		window->AddElement(name);
	});

	HierarchyUI::RegisterPropertyDrawFunction(Material::STATIC_COMPONENT_TYPE_ID, [](IComponent* component, DebugUIWindow* window)
	{
		window->AddElement(std::make_shared<Text>("Material:"));

		Material* comp = (Material*)component;
		auto shader = std::make_shared<Text>(comp->shaderName);
		window->AddElement(shader);

		auto table = std::make_shared<Table>();
		int row = 0;
		for(auto& [name, path] : comp->textures)
		{
			auto nameText = std::make_shared<Text>(const_cast<std::string&>(name));
			auto pathText = std::make_shared<TextEdit>(&path);
			pathText->RegisterCallback([comp](TextEdit* textEdit)
			{
				MaterialSystem* matSys = ECSEngine::GetInstance().systemManager->GetSystem<MaterialSystem>();
				matSys->UpdateMaterial(comp);

			});
			table->AddElement(nameText, row, 1);
			table->AddElement(pathText, row, 2);
			row++;
		}
		window->AddElement(table);
	});

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

	m_propertyDrawFunctions[NameTag::STATIC_COMPONENT_TYPE_ID](ECSEngine::GetInstance().componentManager->GetComponent<NameTag>(entity), &m_propertiesWindow);
	m_propertiesWindow.AddElement(std::make_shared<Separator>());
	m_propertyDrawFunctions[Transform::STATIC_COMPONENT_TYPE_ID](ECSEngine::GetInstance().componentManager->GetComponent<Transform>(entity), &m_propertiesWindow);
	m_propertiesWindow.AddElement(std::make_shared<Separator>());

	for(auto& [typeID, component] : components)
	{
		if(typeID == Transform::STATIC_COMPONENT_TYPE_ID || typeID == NameTag::STATIC_COMPONENT_TYPE_ID)
			continue;

		auto it = m_propertyDrawFunctions.find(typeID);
		if(it != m_propertyDrawFunctions.end())
		{
			it->second(component, &m_propertiesWindow);
			m_propertiesWindow.AddElement(std::make_shared<Separator>());
		}
	}
}




// void HierarchyUI::SetupComponentProperties(ComponentTypeID type, IComponent* component)
// {
// 	if(type == Transform::STATIC_COMPONENT_TYPE_ID)
// 	{
// 		Transform* comp = (Transform*)component;
// 		auto position = std::make_shared<DragVector3>(&comp->lPosition, "Position");
// 		auto scale = std::make_shared<DragVector3>(&comp->lScale, "Scale");

// 		m_propertiesWindow.AddElement(position);
// 		m_propertiesWindow.AddElement(scale);

// 		return;
// 	}
// 	if(type == NameTag::STATIC_COMPONENT_TYPE_ID)
// 	{
// 		auto name = std::make_shared<Text>(((NameTag*)component)->name);
// 		m_propertiesWindow.AddElement(name);

// 		return;
// 	}

// 	if(type == Material::STATIC_COMPONENT_TYPE_ID)
// 	{
// 		Material* comp = (Material*)component;

// 		auto shader = std::make_shared<Text>(comp->shaderName);
// 		m_propertiesWindow.AddElement(shader);

// 		auto table = std::make_shared<Table>();
// 		int row = 0;
// 		for(auto& [name, path] : comp->textures)
// 		{
// 			auto nameText = std::make_shared<Text>(name);
// 			auto pathText = std::make_shared<Text>(path);

// 			table->AddElement(nameText, row, 1);
// 			table->AddElement(pathText, row, 2);
// 			row++;
// 		}
// 		m_propertiesWindow.AddElement(table);
// 	}
// }