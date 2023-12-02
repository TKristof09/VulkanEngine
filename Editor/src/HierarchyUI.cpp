#include "HierarchyUI.hpp"

#include "Application.hpp"
#include "Rendering/TextureManager.hpp"
#include "Rendering/MaterialSystem.hpp"
#include "Core/Events/EventHandler.hpp"
std::unordered_map<std::type_index, PropertyDrawFunction> HierarchyUI::m_propertyDrawFunctions = {};


HierarchyUI::HierarchyUI(Scene* scene, Renderer* renderer, MaterialSystem* materialSystem)
    : m_hierarchyWindow(DebugUIWindow("Hierarchy")),
      m_propertiesWindow(DebugUIWindow("Properties")),
      m_renderer(renderer)
{
    renderer->AddDebugUIWindow(&m_hierarchyWindow);
    renderer->AddDebugUIWindow(&m_propertiesWindow);

    Application::GetInstance()->GetEventHandler()->Subscribe(this, &HierarchyUI::OnEntityCreated);

    HierarchyUI::RegisterPropertyDrawFunction<Transform>(
        [](void* component, DebugUIWindow* window)
        {
            window->AddElement(std::make_shared<Text>("Transform:"));

            auto* comp = (Transform*)component;
            window->AddElement(std::make_shared<DragVector3>(&comp->pos, "Position"));
            window->AddElement(std::make_shared<DragEulerAngles>(&comp->rot, "Rotation"));
            window->AddElement(std::make_shared<DragVector3>(&comp->scale, "Scale", 0.0f, FLT_MAX));
        });


    HierarchyUI::RegisterPropertyDrawFunction<Material>(
        [materialSystem](void* component, DebugUIWindow* window)
        {
            auto* comp = (Material*)component;
            window->AddElement(std::make_shared<Text>("Material:    " + comp->shaderName));


            auto table = std::make_shared<Table>();
            int row    = 0;
            window->AddElement(std::make_shared<Text>("Textures:"));
            for(auto& [name, path] : comp->textures)
            {
                auto nameText = std::make_shared<Text>("\t" + name);
                auto file     = std::make_shared<FileSelector>(&path);
                file->RegisterCallback(
                    [comp, materialSystem](FileSelector* fileSelector)
                    {
                        TextureManager::LoadTexture(fileSelector->GetPath());
                        materialSystem->UpdateMaterial(comp);
                    });

                table->AddElement(nameText, row, 1);
                table->AddElement(file, row, 2);
                row++;
            }
            window->AddElement(table);
        });
}

void HierarchyUI::OnEntityCreated(EntityCreated event)
{
    auto node     = std::make_shared<TreeNode>(event.entity.GetName());
    Entity entity = event.entity;
    node->RegisterCallback(
        [this, entity](TreeNode* node)
        {
            EntitySelectedCallback(entity);
            m_hierarchyWindow.DeselectAll();
            node->SetIsSelected(true);
        });

    if(event.entity.GetParent() == Entity::INVALID_ENTITY)
    {
        m_hierarchyWindow.AddElement(node);
        m_hierarchyTree[event.entity] = node;
    }
    else
    {
        m_hierarchyTree[event.entity.GetParent()]->AddElement(node);
        m_hierarchyTree[event.entity] = node;
    }
}

void HierarchyUI::EntitySelectedCallback(Entity entity)
{
    m_selectedEntity = entity;
    LOG_TRACE("Selected entity: {0}", entity.GetName());


    m_propertiesWindow.Clear();


    DrawComponent<Transform>(entity);
    DrawComponent<Material>(entity);
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
