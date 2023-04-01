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
	camera->AddComponent<Camera>(90.f, 1920/1080, 0.1f, 1000.f);
	t->pos = { 0.0f, 0.0f, 9.0f };

    Entity* e2 = AssimpImporter::LoadFile("models/sponza.fbx", ecs);

	Entity* dlight = ecs->entityManager->CreateEntity();
    dlight->GetComponent<NameTag>()->name = "DLight";
	auto dl = dlight->AddComponent<DirectionalLight>();
	dl->color = Color::Green;
	dl->intensity = 10.0f;




	Entity* lightParent = ecs->entityManager->CreateEntity();
    lightParent->GetComponent<NameTag>()->name = "point lights";
	for(int i = 0; i < 16; ++i)
	{
		for(int j = 0; j < 16; ++j)
		{
			Entity* d = ecs->entityManager->CreateChild(lightParent);
			auto l = d->AddComponent<PointLight>();
			l->color = Color::Red;
			l->intensity = 100.0f;
			l->range = 1000.0f;
			l->attenuation = {1,1,1};
			//l->cutoff = glm::radians(40.f);
			auto dt = d->GetComponent<Transform>();
			dt->pos = {i * 5, 1.0f , j*5};
		}
	}


	//EntityID id2 = ecs->entityManager->CreateChild(id);
	//ecs->componentManager->AddComponent<Mesh>(id2, vertices2, indices2);



	Transform* t2 = e2->GetComponent<Transform>();
	t2->pos = { 0.f, 24.0f, 0.0f };
	t2->scale = {0.1f, 0.1f, 0.1f};
	e2->GetComponent<Transform>()->pos.y -= 200;
	//t2->lPosition = {3.0f, -0.5f,-1.0f};
	//t2->rot = glm::rotate(t2->rot, glm::radians(45.f), glm::vec3(0,1,0));

	//ecs->systemManager->AddSystem<TestSystem>(lightParent->GetEntityID());

	editor.Run();

	return 0;
}
