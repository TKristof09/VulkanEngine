#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
class Window
{
public:
    Window(uint32_t width, uint32_t height, const std::string& title);
    ~Window();



private:
    uint32_t    m_width;
    uint32_t    m_height;
    std::string m_title;
    GLFWwindow* m_window;
};