#include "Window.hpp"

Window::Window(uint32_t width, uint32_t height, const std::string& title):
m_width(width), m_height(height), m_title(title)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    m_window = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);

}

Window::~Window()
{
    glfwDestroyWindow(m_window);
    glfwTerminate();
}
