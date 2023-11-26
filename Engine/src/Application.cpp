#include "Application.hpp"

#include <filesystem>

#include "Rendering/MaterialSystem.hpp"
#include "Rendering/TextureManager.hpp"
#include "ECS/Core.hpp"
#include "Utils/Time.hpp"
#include "Core/Events/EventHandler.hpp"

Application* Application::s_instance = nullptr;

Application::Application(uint32_t width, uint32_t height, uint32_t frameRate, const std::string& title) : m_frameTime(1.0 / frameRate)
{
    s_instance = this;

    std::filesystem::current_path("H:/Programming/VulkanEngine");

    Log::Init();
    LOG_INFO("Cwd: {0}", std::filesystem::current_path().string());

    Instrumentor::Get().BeginSession(title);


    m_eventHandler      = std::make_unique<EventHandler>();
    m_currentScene      = std::make_unique<Scene>();
    m_currentScene->ecs = std::make_unique<ECS>();

    m_window = std::make_shared<Window>(width, height, title);

    m_renderer = std::make_unique<Renderer>(m_window);

    m_materialSystem = std::make_unique<MaterialSystem>();
}

Application::~Application()
{
    vkDeviceWaitIdle(VulkanContext::GetDevice());
    m_renderer.reset();
    m_materialSystem.reset();
    m_currentScene.reset();
    m_eventHandler.reset();

    VulkanContext::Cleanup();

    glfwTerminate();
}

void Application::SetScene(Scene* scene)
{
    m_currentScene = std::unique_ptr<Scene>(scene);
    SceneSwitchedEvent e;
    e.newScene = scene;
    m_eventHandler->Send<SceneSwitchedEvent>(e);
}
void Application::Run()
{
    GLFWwindow* w = m_window->GetWindow();

    double lastTime = Time::GetTime();

    while(!m_window->ShouldClose())
    {
        double startTime = Time::GetTime();
        double deltaTime = startTime - lastTime;
        lastTime         = startTime;


        glfwPollEvents();  // TODO this blocks while you hold the title bar(or the resize cursor), only fix seems to be to render on another thread

        if(glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            break;

        m_eventHandler->DispatchEvents();


        Time::SetDelta(deltaTime);
        m_currentScene->ecs->Update(deltaTime);

        m_renderer->Render(deltaTime);
    }
}
