#include <iostream>
#include <Engine.hpp>
#include <filesystem>

#include "GLFW/glfw3.h"
#include <vector>
#include "ECS/CoreComponents/Mesh.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "ECS/CoreComponents/Material.hpp"

#include "Utils/Color.hpp"
#include "Utils/AssimpImporter.hpp"
#include "TextureManager.hpp"

#include "HierarchyUI.hpp"
#include "Application.hpp"
#include "Utils/FileDialog/FileDialog.hpp"

int main()
{
	Application editor(1920, 1080, 60, "Editor");

	Scene scene = editor.GetScene();
	
	ECSEngine* ecs = scene.ecs;
	HierarchyUI hierarchyUi(&scene, editor.GetRenderer(), editor.GetMaterialSystem());

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

	

	Entity* e1 = ecs->entityManager->CreateEntity();
	e1->AddComponent<Mesh>(vertices, indices);

	AssimpImporter importer;
	Entity* e2 = importer.LoadFile("models/cube.obj", ecs);
	//EntityID id2 = ecs->entityManager->CreateChild(id);
	//ecs->componentManager->AddComponent<Mesh>(id2, vertices2, indices2);

	Entity* camera = ecs->entityManager->CreateEntity();
	Transform* t = camera->AddComponent<Transform>();
	camera->AddComponent<Camera>(90.f, 1920/1080, 0.001f, 100.f);
	t->lPosition = { 0.0f, 2.0f, 10.0f };

	Transform* t1 = e1->GetComponent<Transform>();
	Transform* t2 = e2->GetComponent<Transform>();
	t2->lPosition = { 3.0f, -2.0f, 0.0f };
	//t1->lScale = {10.0f, 10.0f, 10.0f };
	//t2->lPosition = {3.0f, -0.5f,-1.0f};


	Material* mat1 = e1->AddComponent<Material>();
	mat1->shaderName = "base";

	auto texturePath = FileDialog::OpenFile("Images\0*.jpg;*.jpeg;*.png\0");
	TextureManager::LoadTexture(texturePath);
	mat1->textures["albedo"] = texturePath;

	//Material* mat2 = engine->componentManager->AddComponent<Material>(id2);
	//mat2->shaderName = "base";
	//TextureManager::LoadTexture("./textures/texture.jpg");
	//mat2->textures["albedo"] = "./textures/texture.jpg";

	editor.Run();
	
	return 0;
}
