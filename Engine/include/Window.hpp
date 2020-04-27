#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Window
{
public:
    Window(uint32_t width, uint32_t height, const eastl::string& title);
    ~Window();

    GLFWwindow* GetWindow() const {   return m_window;  };
	bool IsResized() const { return m_resized; };

	void SetResized(bool value) { m_resized = value; };
	void _Resized(uint32_t width, uint32_t height)
	{
		m_width = width;
		m_height = height;
		m_resized = true;
	}

	uint32_t GetWidth() const { return m_width; };
	uint32_t GetHeight() const { return m_height; };



private:
    uint32_t        m_width;
    uint32_t        m_height;
    eastl::string     m_title;
    GLFWwindow*     m_window;
	bool			m_resized;
};
