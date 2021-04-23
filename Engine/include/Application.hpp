#pragma once
#include "Window.hpp"


class ECSEngine;

class Application
{
public:
	Application(uint32_t width, uint32_t height, uint32_t frameRate, const std::string& title = "Vulkan Application");
	~Application();
	void Run();
private:
	ECSEngine* m_ecsEngine;
	std::shared_ptr<Window> m_window;
	double m_frameTime;

};
