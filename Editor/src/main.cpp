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
#include "Rendering/TextureManager.hpp"

#include "HierarchyUI.hpp"
#include "Application.hpp"
#include "Utils/FileDialog/FileDialog.hpp"


#include "TestSystem.hpp"

int main()
{

	Application editor(1920, 1080, 60, "Editor");

	Scene scene = editor.GetScene();

	ECSEngine* ecs = scene.ecs;
	HierarchyUI hierarchyUi(&scene, editor.GetRenderer(), editor.GetMaterialSystem());



	Entity* camera = ecs->entityManager->CreateEntity();
	Transform* t = camera->AddComponent<Transform>();
	camera->AddComponent<Camera>(90.f, 1920/1080, 0.1f, 100.f);
	t->pos = { 0.0f, 2.0f, 10.0f };



	Entity* e2 = AssimpImporter::LoadFile("models/sponza_one.fbx", ecs);


	for(int i = 0; i < 16; ++i)
	{
		for(int j = 0; j < 16; ++j)
		{

			Entity* d = ecs->entityManager->CreateEntity();
			auto l = d->AddComponent<PointLight>();
			l->color = Color::Red;
			l->intensity = 5.0f;
			l->range = 10.0f;
			l->attenuation = {1,1,1};
			//l->cutoff = glm::radians(40.f);
			auto dt = d->GetComponent<Transform>();
			dt->pos = {i * 5, 0.0f , j};
		}
	}



	//EntityID id2 = ecs->entityManager->CreateChild(id);
	//ecs->componentManager->AddComponent<Mesh>(id2, vertices2, indices2);




	Transform* t2 = e2->GetComponent<Transform>();
	t2->pos = { 0.f, 24.0f, 0.0f };
	t2->scale = {0.1f, 0.1f, 0.1f};
	//t2->lPosition = {3.0f, -0.5f,-1.0f};
	//t2->rot = glm::rotate(t2->rot, glm::radians(45.f), glm::vec3(0,1,0));

	//ecs->systemManager->AddSystem<TestSystem>(d->GetEntityID());

	editor.Run();

	return 0;
}
