#pragma once
#include "Rendering/Window.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Events/CoreEvents.hpp"

class ECS;
class MaterialSystem;
class EventHandler;
class Renderer;

class Application
{
public:
    static Application* GetInstance() { return s_instance; }

    Application(uint32_t width, uint32_t height, uint32_t frameRate, const std::string& title = "Vulkan Application");

    ~Application();

    void Run();

    void SetScene(Scene* scene);


    Scene* GetScene() { return m_currentScene.get(); }

    Renderer* GetRenderer() const { return m_renderer.get(); }

    MaterialSystem* GetMaterialSystem() const { return m_materialSystem.get(); }


    std::shared_ptr<Window> GetWindow() const { return m_window; }

    EventHandler* GetEventHandler() const { return m_eventHandler.get(); }

private:
    std::unique_ptr<Scene> m_currentScene;
    std::shared_ptr<Window> m_window;
    std::unique_ptr<EventHandler> m_eventHandler;
    std::unique_ptr<Renderer> m_renderer;
    double m_frameTime;
    std::unique_ptr<MaterialSystem> m_materialSystem;

    static Application* s_instance;
};
