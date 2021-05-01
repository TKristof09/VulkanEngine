#pragma once
#include "Window.hpp"
#include "Rendering/Renderer.hpp"


class ECSEngine;

class Application
{
public:
	Application(uint32_t width, uint32_t height, uint32_t frameRate, const std::string& title = "Vulkan Application");
	~Application();
	void Run();

	void SetECS(ECSEngine* ecs) { m_currentECS = ecs; }
private:
	ECSEngine* m_currentECS;
	std::shared_ptr<Window> m_window;
	Renderer* m_renderer;
	double m_frameTime;

};
