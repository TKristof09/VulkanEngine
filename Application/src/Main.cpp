#include "main.h"
#include <iostream>
#include <Engine.hpp>
#include "GLFW/glfw3.h"
#include <vector>
#include "ECS/CoreComponents/Mesh.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "Utils/Color.hpp"

void run()
{
    std::cout << "Hello from run" << std::endl;
}
int main()
{
	Log::Init();

    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, -1.f}, {1.0f, 0.0f}},
        {{0.5f, -0.5f, -1.f}, {0.0f, 1.0f}},
        {{0.5f, 0.5f, -1.f}, {1.0f, 1.0f}},
        {{-0.5f, 0.5f, -1.f}, {0.0f, 0.0f}}
    };

    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0
    };
    std::shared_ptr<Window> window = std::make_shared<Window>(1280, 720, "Vulkan Application");
    std::cout << "Hello from main" << std::endl;
    run();
    // TODO
    GLFWwindow* w = window->GetWindow();
    ECSEngine* engine = new ECSEngine();
    engine->systemManager->AddSystem<RendererSystem>(window);
    EntityID id = engine->entityManager->CreateEntity();
    engine->componentManager->AddComponent<Mesh>(id, vertices, indices);
    EntityID cameraID = engine->entityManager->CreateEntity();
    engine->componentManager->AddComponent<Transform>(cameraID);
    engine->componentManager->AddComponent<Camera>(cameraID, 90.f, window->GetWidth()/window->GetHeight(), 0.001f, 100.f);

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
