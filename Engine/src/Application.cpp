#include "Application.hpp"

#include <filesystem>

#include "ECS/CoreSystems/TransformHierarchySystem.hpp"
#include "Rendering/MaterialSystem.hpp"
#include "TextureManager.hpp"
#include "ECS/ECSEngine.hpp"
#include "ECS/SystemManager.hpp"
#include "Utils/Time.hpp"

Application::Application(uint32_t width, uint32_t height, uint32_t frameRate, const std::string& title):
	m_frameTime(1.0 / frameRate)
{
	Log::Init();

	std::filesystem::current_path("G:/Programozas/C++/VulkanEngine");
	LOG_INFO("Cwd: {0}", std::filesystem::current_path().string());

	m_currentECS = new ECSEngine();
	
	m_window = std::make_shared<Window>(width, height, title);

	m_renderer = new Renderer(m_window, m_currentECS);

	m_currentECS->systemManager->AddSystem<TransformHierarchySystem>();

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
			m_currentECS->FixedUpdate(m_frameTime);
			unprocessedTime -= m_frameTime;
		}

		Time::SetDelta(deltaTime);
		m_currentECS->Update(deltaTime);

		m_renderer->Render(deltaTime);
		
	}
}
