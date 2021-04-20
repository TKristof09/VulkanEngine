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
#include "TextureManager.hpp"

#include "HierarchyUI.hpp"

int main()
{
	Log::Init();
	std::shared_ptr<Window> window = std::make_shared<Window>(1280, 720, "Vulkan Application");
	GLFWwindow* w = window->GetWindow();
	ECSEngine* engine = &ECSEngine::GetInstance();

	engine->systemManager->AddSystem<RendererSystem>(window);
	engine->systemManager->AddSystem<MaterialSystem>();
	engine->systemManager->AddSystem<TransformHierarchySystem>();
	
	TextureManager::LoadTexture("./textures/error.jpg");

	HierarchyUI ui(engine->systemManager->GetSystem<RendererSystem>());
	
	
	std::vector<Vertex> vertices = {
		{{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
		{{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
		{{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
		{{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}
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


	EntityID id = engine->entityManager->CreateEntity();
	engine->componentManager->AddComponent<Mesh>(id, vertices, indices);

	AssimpImporter importer;
	EntityID id2 = importer.LoadFile("models/cube.obj", engine);
	//EntityID id2 = engine->entityManager->CreateChild(id);
	//engine->componentManager->AddComponent<Mesh>(id2, vertices2, indices2);

	EntityID cameraID = engine->entityManager->CreateEntity();
	Transform* t = engine->componentManager->AddComponent<Transform>(cameraID);
	engine->componentManager->AddComponent<Camera>(cameraID, 90.f, window->GetWidth() / window->GetHeight(), 0.001f, 100.f);
	t->lPosition = { 0.0f, 2.0f, 10.0f };

	Transform* t1 = engine->componentManager->AddComponent<Transform>(id);
	Transform* t2 = engine->componentManager->GetComponent<Transform>(id2);
	t2->lPosition = { 3.0f, -2.0f, 0.0f };
	//t1->lScale = {10.0f, 10.0f, 10.0f };
	//t2->lPosition = {3.0f, -0.5f,-1.0f};


	Material* mat1 = engine->componentManager->AddComponent<Material>(id);
	mat1->shaderName = "base";
	TextureManager::LoadTexture("./textures/texture.jpg");
	mat1->textures["albedo"] = "./textures/texture.jpg";

	//Material* mat2 = engine->componentManager->AddComponent<Material>(id2);
	//mat2->shaderName = "base";
	//TextureManager::LoadTexture("./textures/texture.jpg");
	//mat2->textures["albedo"] = "./textures/texture.jpg";


	while(!glfwWindowShouldClose(w))
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
