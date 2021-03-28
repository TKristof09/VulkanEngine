#include "main.h"
#include <iostream>
#include <Engine.hpp>
#include "GLFW/glfw3.h"
#include <vector>
#include "ECS/CoreComponents/Mesh.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "ECS/CoreComponents/Material.hpp"
#include "ECS/CoreSystems/TransformHierarchySystem.hpp"
#include "ECS/CoreSystems/MaterialSystem.hpp"
#include "Utils/Color.hpp"
#include "Utils/AssimpImporter.hpp"

void run()
{
    std::cout << "Hello from run" << std::endl;
}
int main()
{
	Log::Init();

    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
		{{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
		{{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},
		{{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}}
	};
	std::vector<Vertex> vertices2 = {
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
		{{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
		{{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},
	};

    std::vector<uint32_t> indices = {
		0, 1, 2, 2, 3, 0,
    };
    std::vector<uint32_t> indices2 = {
		0, 1, 2
    };

    std::shared_ptr<Window> window = std::make_shared<Window>(1280, 720, "Vulkan Application");
    std::cout << "Hello from main" << std::endl;
    run();
    // TODO
    GLFWwindow* w = window->GetWindow();
    ECSEngine* engine = new ECSEngine();
    engine->systemManager->AddSystem<RendererSystem>(window);
	engine->systemManager->AddSystem<MaterialSystem>();
	engine->systemManager->AddSystem<TransformHierarchySystem>();

    EntityID id = engine->entityManager->CreateEntity();
    engine->componentManager->AddComponent<Mesh>(id, vertices, indices);
	EntityID id2 = engine->entityManager->CreateChild(id);
    engine->componentManager->AddComponent<Mesh>(id2, vertices2, indices2);
	AssimpImporter importer;
	//importer.LoadFile("models/chalet.obj", engine);
    EntityID cameraID = engine->entityManager->CreateEntity();
    Transform* t = engine->componentManager->AddComponent<Transform>(cameraID);
    engine->componentManager->AddComponent<Camera>(cameraID, 90.f, window->GetWidth()/window->GetHeight(), 0.001f, 100.f);
	t->wPosition = {0.0f, 0.0f, -5.0f};

    Transform* t1 = engine->componentManager->AddComponent<Transform>(id);
    Transform* t2 = engine->componentManager->AddComponent<Transform>(id2);
	t1->wPosition = {3.0f, 2.0f,0.0f};
	t2->lPosition = {0.0f, -0.5f,-1.0f};

	Texture albedo1("./textures/chalet.jpg");
	Texture albedo2("./textures/texture.jpg");

	Material* mat1 = engine->componentManager->AddComponent<Material>(id);
	mat1->shaderName = "base";
	mat1->textures["albedo"] = std::move(albedo1);

	Material* mat2 = engine->componentManager->AddComponent<Material>(id2);
	mat2->shaderName = "base";
	mat2->textures["albedo"] = std::move(albedo2);
    while(!glfwWindowShouldClose(w) )
    {
		glfwPollEvents();
		if(glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			break;
		engine->Update(1.f);
    }
    glfwTerminate();
    //std::cin.get();
    return 0;
}
