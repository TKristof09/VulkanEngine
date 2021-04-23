#include "Application.hpp"

#include <filesystem>

#include "ECS/CoreSystems/TransformHierarchySystem.hpp"
#include "ECS/CoreSystems/MaterialSystem.hpp"
#include "ECS/CoreSystems/RendererSystem.hpp"
#include "TextureManager.hpp"
#include "ECS/ECSEngine.hpp"
#include "ECS/SystemManager.hpp"
#include "Utils/Time.hpp"

Application::Application(uint32_t width, uint32_t height, uint32_t frameRate, const std::string& title):
	m_frameTime(1.0 / frameRate)
{
	Log::Init();

	std::filesystem::current_path("C:/Users/tothk/Desktop/VulkanEngine");
	LOG_INFO("Cwd: {0}", std::filesystem::current_path().string());

	m_window = std::make_shared<Window>(width, height, title);

	m_ecsEngine = &ECSEngine::GetInstance();

	m_ecsEngine->systemManager->AddSystem<RendererSystem>(m_window);
	m_ecsEngine->systemManager->AddSystem<MaterialSystem>();
	m_ecsEngine->systemManager->AddSystem<TransformHierarchySystem>();

	TextureManager::LoadTexture("./textures/error.jpg");
}

Application::~Application()
{
	glfwTerminate();
}

void Application::Run()
{
	GLFWwindow* w = m_window->GetWindow();

	double lastTime = Time::GetTime();
	double unprocessedTime = 0;

	while(!m_window->ShouldClose())
	{
		double startTime = Time::GetTime();
		double deltaTime = startTime - lastTime;
		lastTime = startTime;
		
		unprocessedTime += deltaTime;
		
		glfwPollEvents();
		if(glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			break;
		while(unprocessedTime >= m_frameTime)
		{
			Time::SetDelta(m_frameTime);
			m_ecsEngine->FixedUpdate(m_frameTime);
			unprocessedTime -= m_frameTime;
		}

		Time::SetDelta(deltaTime);
		m_ecsEngine->Update(deltaTime);
		
	}
}
