#pragma once
#include "Rendering/Window.hpp"
#include "Rendering/Renderer.hpp"
#include "Core/Scene/Scene.hpp"

class ECSEngine;

class Application
{
public:
	static Application* GetInstance() { return s_instance; }
	
	Application(uint32_t width, uint32_t height, uint32_t frameRate, const std::string& title = "Vulkan Application");

	~Application();

	void Run();

	void SetScene(const Scene& scene) { m_currentScene = scene; }

	Scene& GetScene() { return m_currentScene; }

	Renderer* GetRenderer() const { return m_renderer; }

	MaterialSystem* GetMaterialSystem() const	{ return m_materialSystem; }


	std::shared_ptr<Window> GetWindow() const { return m_window; }
private:
	Scene m_currentScene;
	std::shared_ptr<Window> m_window;
	Renderer* m_renderer;
	double m_frameTime;
	MaterialSystem* m_materialSystem;

	static Application* s_instance;
};
