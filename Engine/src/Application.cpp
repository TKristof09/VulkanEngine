#include "Application.hpp"

#include <filesystem>

#include "ECS/CoreSystems/TransformHierarchySystem.hpp"
#include "Rendering/MaterialSystem.hpp"
#include "TextureManager.hpp"
#include "ECS/ECSEngine.hpp"
#include "ECS/SystemManager.hpp"
#include "Utils/Time.hpp"

Application* Application::s_instance = nullptr;

Application::Application(uint32_t width, uint32_t height, uint32_t frameRate, const std::string& title):
	m_frameTime(1.0 / frameRate)
{
	s_instance = this;
	
	Log::Init();

	std::filesystem::current_path("G:/Programozas/C++/VulkanEngine");
	LOG_INFO("Cwd: {0}", std::filesystem::current_path().string());

	m_currentScene.eventHandler = new EventHandler();
	
	m_currentScene.ecs = new ECSEngine(m_currentScene);
	
	m_window = std::make_shared<Window>(width, height, title);

	m_renderer = new Renderer(m_window, &m_currentScene);

	m_materialSystem = new MaterialSystem(&m_currentScene, m_renderer);
	
	m_currentScene.ecs->systemManager->AddSystem<TransformHierarchySystem>();

	TextureManager::LoadTexture("./textures/error.jpg");
}

Application::~Application()
{
	
	vkDeviceWaitIdle(VulkanContext::GetDevice());
	delete m_currentScene.ecs;
	delete m_currentScene.eventHandler;
	delete m_materialSystem;
	delete m_renderer;
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

		glfwPollEvents(); // TODO this blocks while you hold the title bar(or the resize cursor), only fix seems to be to render on another thread
		
		if(glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			break;

		m_currentScene.eventHandler->DispatchEvents();
		while(unprocessedTime >= m_frameTime)
		{
			Time::SetDelta(m_frameTime);
			m_currentScene.ecs->FixedUpdate(m_frameTime);
			unprocessedTime -= m_frameTime;
		}

		Time::SetDelta(deltaTime);
		m_currentScene.ecs->Update(deltaTime);

		m_renderer->Render(deltaTime);
		
	}
}
