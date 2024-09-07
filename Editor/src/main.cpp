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


void sponza_test(ECS* ecs)
{
    Entity camera = ecs->CreateEntity("MainCamera");
    camera.EmplaceComponent<Camera>(90.f, 1920 / 1080.0f, 0.1f);
    camera.GetComponentMut<Transform>()->pos = {0.0f, 60.0f, -10.0f};
    // t->rot = glm::rotate(t->rot, glm::radians(-90.f), glm::vec3(1,0,0));


    Entity e2                              = AssimpImporter::LoadFile("models/sponza_smooth.fbx", ecs);
    e2.GetComponentMut<Transform>()->scale = {0.1f, 0.1f, 0.1f};

    // Entity* dlight                        = ecs->entityManager->CreateEntity();
    // dlight->GetComponent<NameTag>()->name = "DLight";
    // auto dl                               = dlight->AddComponent<DirectionalLight>();
    // dl->color                             = Color::White;
    // dl->intensity                         = 1.0f;
    // Transform* t2 = dlight->GetComponent<Transform>();
    // t2->rot       = glm::rotate(t2->rot, glm::radians(-45.f), glm::vec3(1, 0, 0));


    Entity lightParent        = ecs->CreateEntity("point lights");
    std::vector<Color> colors = {Color::Red, Color::Blue, Color::Green, Color::White};
    constexpr int gridSize    = 0;
    for(int i = 0; i < gridSize; ++i)
    {
        for(int j = 0; j < gridSize; ++j)
        {
            Entity d = lightParent.CreateChild("Light");
            PointLight l{};
            int index     = i * gridSize + j;
            l.color       = colors[index % colors.size()];
            l.intensity   = 100.0f;
            l.range       = 100.0f;
            l.attenuation = {1, 0, 0};
            // l.cutoff = glm::radians(40.f);
            d.SetComponent<PointLight>(l);
            auto* dt = d.GetComponentMut<Transform>();
            dt->pos  = {(i - gridSize / 2.f) * 10, 1.0f, (j - gridSize / 2.f) * 10};
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


    ecs->AddSystem<TestSystem>(5, 5);

    /* Entity dlight = ecs->CreateEntity("DLight");
     DirectionalLight dl{};
     dl.color     = Color::White;
     dl.intensity = 1.0f;
     dlight.SetComponent<DirectionalLight>(dl);
     auto* t2 = dlight.GetComponentMut<Transform>();
     t2->rot  = glm::rotate(t2->rot, glm::radians(-45.f), glm::vec3(1, 0, 0));*/

    Entity slight = ecs->CreateEntity("SLight");
    SpotLight sl{};
    sl.color       = Color::Red;
    sl.intensity   = 500.0f;
    sl.range       = 1000.0f;
    sl.attenuation = {1, 0, 0};
    sl.cutoff      = glm::cos(glm::radians(45.f));
    slight.SetComponent<SpotLight>(sl);
    auto* ts = slight.GetComponentMut<Transform>();
    // ts->rot                               = glm::rotate(t2->rot, glm::radians(-45.f), glm::vec3(1, 0, 0));
    ts->pos  = {3.0f, 1.f, 0.0f};
}

void pbr_spheres(ECS* ecs)
{
    Entity camera = ecs->CreateEntity("MainCamera");
    camera.EmplaceComponent<Camera>(90.f, 1920 / 1080.0f, 0.1f);
    camera.GetComponentMut<Transform>()->pos = {0.0f, 4.0f, 10.0f};
    // t->rot = glm::rotate(t->rot, glm::radians(-90.f), glm::vec3(1,0,0));


    Entity ground                              = AssimpImporter::LoadFile("models/cube.obj", ecs);
    auto* gt                                   = ground.GetComponentMut<Transform>();
    gt->scale                                  = {20.f, 0.1f, 20.f};
    gt->pos                                    = {0.f, -2.f, 0.f};
    ground.GetComponentMut<Material>()->albedo = glm::vec3(0.1f, 0.7f, 0.05f);


    Entity parent                  = ecs->CreateEntity("Spheres");
    constexpr int gridSize         = 5;
    std::vector<std::string> names = {
        "models/rusty_sphere.fbx",
        "models/grassy_sphere.fbx",
        "models/aluminum_sphere.fbx",
        "models/brick_sphere.fbx",
        "models/gold_sphere.fbx",
        "models/floor_sphere.fbx",
        //"models/stylised_fur_sphere.fbx"
    };
    for(int i = 0; i < gridSize; ++i)
    {
        for(int j = 0; j < gridSize; ++j)
        {
            int index = i * gridSize + j;
            Entity e  = AssimpImporter::LoadFile(names[rand() % names.size()], ecs, &parent);
            auto* tt  = e.GetComponentMut<Transform>();
            tt->pos   = {(i - gridSize / 2.f) * 3, 0.0f, (j - gridSize / 2.f) * 3};
            tt->scale = {1, 1, 1};
        }
    }
    parent = ecs->CreateEntity("Cubes");
    for(int i = 0; i < gridSize; ++i)
    {
        for(int j = 0; j < gridSize; ++j)
        {
            int index      = i * gridSize + j;
            Entity e       = AssimpImporter::LoadFile("models/cube.obj", ecs, &parent);
            auto* tt       = e.GetComponentMut<Transform>();
            tt->pos        = {(i - gridSize / 2.f) * 3, j * 3, -10.0f};
            tt->scale      = {1, 1, 1};
            auto* mat      = e.GetComponentMut<Material>();
            mat->albedo    = glm::vec3(1.0f, 0.0f, 0.0f);
            mat->roughness = glm::clamp(i / ((float)gridSize), 0.05f, 1.0f);  // roughness 0 looks odd in direct lighting
            mat->metallic  = j / ((float)gridSize);
        }
    }


    Entity lightParent        = ecs->CreateEntity("point lights");
    std::vector<Color> colors = {Color::Red, Color::Blue, Color::Green, Color::White};
    constexpr int lgridSize   = 5;
    for(int i = 0; i < lgridSize; ++i)
    {
        for(int j = 0; j < lgridSize; ++j)
        {
            Entity d = lightParent.CreateChild("Light");
            PointLight l{};
            int index     = i * lgridSize + j;
            l.color       = colors[index % colors.size()];
            l.intensity   = 200.0f;
            l.range       = 100.0f;
            l.attenuation = {1, 0, 0};
            // l.cutoff = glm::radians(40.f);
            d.SetComponent<PointLight>(l);
            auto* dt = d.GetComponentMut<Transform>();
            dt->pos  = {(i - lgridSize / 2.f) * 3, 1.5f, (j - lgridSize / 2.f) * 3};
        }
    }
    // ecs->AddSystem<TestSystem>(2, 2);
    {
        Entity dlight = ecs->CreateEntity("DLight");
        DirectionalLight dl{};
        dl.color     = Color::White;
        dl.intensity = 1.0f;
        dlight.SetComponent<DirectionalLight>(dl);
        auto* t2 = dlight.GetComponentMut<Transform>();
        t2->rot  = glm::rotate(t2->rot, glm::radians(-45.f), glm::vec3(1, 0, 0));
    }
    /*{
        Entity dlight = ecs->CreateEntity("DLight2");
        DirectionalLight dl{};
        dl.color     = Color::Blue;
        dl.intensity = 10.0f;
        dlight.SetComponent<DirectionalLight>(dl);
        auto* t2 = dlight.GetComponentMut<Transform>();
        t2->rot  = glm::rotate(t2->rot, glm::radians(-45.f), glm::vec3(1, 0, 0));
    }*/
    Entity slight = ecs->CreateEntity("SLight");
    SpotLight sl{};
    sl.color       = Color::Red;
    sl.intensity   = 500.0f;
    sl.range       = 1000.0f;
    sl.attenuation = {1, 0, 0};
    sl.cutoff      = glm::cos(glm::radians(45.f));
    slight.SetComponent<SpotLight>(sl);
    auto* ts = slight.GetComponentMut<Transform>();
    // ts->rot                               = glm::rotate(t2->rot, glm::radians(-45.f), glm::vec3(1, 0, 0));
    ts->pos  = {3.0f, 1.f, 0.0f};
}

void bistro_ext(ECS* ecs)
{
    Entity camera = ecs->CreateEntity("MainCamera");
    camera.EmplaceComponent<Camera>(90.f, 1920 / 1080.0f, 0.1f);
    camera.GetComponentMut<Transform>()->pos = {0.0f, 60.0f, -10.0f};
    // t->rot = glm::rotate(t->rot, glm::radians(-90.f), glm::vec3(1,0,0));


    Entity e2                              = AssimpImporter::LoadFile("models/Bistro_v5_2/BistroExterior.fbx", ecs);
    e2.GetComponentMut<Transform>()->scale = {0.1f, 0.1f, 0.1f};

    // Entity* dlight                        = ecs->entityManager->CreateEntity();
    // dlight->GetComponent<NameTag>()->name = "DLight";
    // auto dl                               = dlight->AddComponent<DirectionalLight>();
    // dl->color                             = Color::White;
    // dl->intensity                         = 1.0f;
    // Transform* t2 = dlight->GetComponent<Transform>();
    // t2->rot       = glm::rotate(t2->rot, glm::radians(-45.f), glm::vec3(1, 0, 0));


    Entity lightParent        = ecs->CreateEntity("point lights");
    std::vector<Color> colors = {Color::Red, Color::Blue, Color::Green, Color::White};
    constexpr int gridSize    = 0;
    for(int i = 0; i < gridSize; ++i)
    {
        for(int j = 0; j < gridSize; ++j)
        {
            Entity d = lightParent.CreateChild("Light");
            PointLight l{};
            int index     = i * gridSize + j;
            l.color       = colors[index % colors.size()];
            l.intensity   = 100.0f;
            l.range       = 100.0f;
            l.attenuation = {1, 0, 0};
            // l.cutoff = glm::radians(40.f);
            d.SetComponent<PointLight>(l);
            auto* dt = d.GetComponentMut<Transform>();
            dt->pos  = {(i - gridSize / 2.f) * 10, 1.0f, (j - gridSize / 2.f) * 10};
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


    ecs->AddSystem<TestSystem>(5, 5);

    /* Entity dlight = ecs->CreateEntity("DLight");
     DirectionalLight dl{};
     dl.color     = Color::White;
     dl.intensity = 1.0f;
     dlight.SetComponent<DirectionalLight>(dl);
     auto* t2 = dlight.GetComponentMut<Transform>();
     t2->rot  = glm::rotate(t2->rot, glm::radians(-45.f), glm::vec3(1, 0, 0));*/

    Entity slight = ecs->CreateEntity("SLight");
    SpotLight sl{};
    sl.color       = Color::Red;
    sl.intensity   = 500.0f;
    sl.range       = 1000.0f;
    sl.attenuation = {1, 0, 0};
    sl.cutoff      = glm::cos(glm::radians(45.f));
    slight.SetComponent<SpotLight>(sl);
    auto* ts = slight.GetComponentMut<Transform>();
    // ts->rot                               = glm::rotate(t2->rot, glm::radians(-45.f), glm::vec3(1, 0, 0));
    ts->pos  = {3.0f, 1.f, 0.0f};
}
int main()
{
    Application editor(1920, 1080, 60, "Editor");

    Scene* scene = editor.GetScene();

    ECS* ecs = scene->GetECS();
    HierarchyUI hierarchyUi(scene, editor.GetRenderer(), editor.GetMaterialSystem());

    int sceneId = 1;
    switch(sceneId)
    {
    case 1:
        sponza_test(ecs);
        break;
    case 2:
        pbr_spheres(ecs);
        break;
    case 3:
        bistro_ext(ecs);
    default:
        return -1;
    }
    ecs->EnableRESTEditor();
    editor.Run();

    return 0;
}
