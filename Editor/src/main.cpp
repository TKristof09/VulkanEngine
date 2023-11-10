#include <iostream>
#include <Engine.hpp>
#include <filesystem>

#include "GLFW/glfw3.h"
#include <vector>
#include "ECS/CoreComponents/Mesh.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "ECS/CoreComponents/Material.hpp"

#include "Rendering/RenderGraph/RenderGraph.hpp"
#include "Utils/Color.hpp"
#include "Utils/AssimpImporter.hpp"
#include "Rendering/TextureManager.hpp"

#include "HierarchyUI.hpp"
#include "Application.hpp"
#include "Utils/FileDialog/FileDialog.hpp"


#include "TestSystem.hpp"


void sponza_test(ECSEngine* ecs)
{
    Entity* camera = ecs->entityManager->CreateEntity();
    Transform* t   = camera->AddComponent<Transform>();
    camera->AddComponent<Camera>(90.f, 1920 / 1080.0f, 0.1f);
    t->pos = {0.0f, 60.0f, -10.0f};
    // t->rot = glm::rotate(t->rot, glm::radians(-90.f), glm::vec3(0,1,0));

    Entity* e2                           = AssimpImporter::LoadFile("models/sponza.fbx", ecs);
    e2->GetComponent<Transform>()->scale = {0.01f, 0.01f, 0.01f};

    Entity* dlight                        = ecs->entityManager->CreateEntity();
    dlight->GetComponent<NameTag>()->name = "DLight";
    auto dl                               = dlight->AddComponent<DirectionalLight>();
    dl->color                             = Color::White;
    dl->intensity                         = 1.0f;


    Entity* lightParent                        = ecs->entityManager->CreateEntity();
    lightParent->GetComponent<NameTag>()->name = "point lights";
    std::vector<Color> colors                  = {Color::Red, Color::Blue, Color::Green, Color::White};
    constexpr int gridSize                     = 9;
    for(int i = 0; i < gridSize; ++i)
    {
        for(int j = 0; j < gridSize; ++j)
        {
            Entity* d      = ecs->entityManager->CreateChild(lightParent);
            auto l         = d->AddComponent<PointLight>();
            int index      = i * gridSize + j;
            l->color       = colors[index % colors.size()];
            l->intensity   = 50.0f;
            l->range       = 100.0f;
            l->attenuation = {1, 0, 0};
            // l->cutoff = glm::radians(40.f);
            auto dt        = d->GetComponent<Transform>();
            dt->pos        = {(i - gridSize / 2.f) * 25, 1.0f, (j - gridSize / 2.f) * 25};
        }
    }


    // EntityID id2 = ecs->entityManager->CreateChild(id);
    // ecs->componentManager->AddComponent<Mesh>(id2, vertices2, indices2);


    /*
    Transform* t2 = e2->GetComponent<Transform>();
    t2->pos = { 0.f, 24.0f, 0.0f };
    t2->scale = {0.1f, 0.1f, 0.1f};
    e2->GetComponent<Transform>()->pos.y -= 200;
    */
    // t2->lPosition = {3.0f, -0.5f,-1.0f};
    Transform* t2 = dlight->GetComponent<Transform>();
    t2->rot       = glm::rotate(t2->rot, glm::radians(-45.f), glm::vec3(1, 0, 0));

    ecs->systemManager->AddSystem<TestSystem>();
}

int main()
{

	Application editor(1920, 1080, 60, "Editor");

	Scene scene = editor.GetScene();

	ECSEngine* ecs = scene.ecs;
	HierarchyUI hierarchyUi(&scene, editor.GetRenderer(), editor.GetMaterialSystem());

    sponza_test(ecs);

	editor.Run();

	return 0;
}
