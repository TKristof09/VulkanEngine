#include "Window.hpp"
#include <memory>

static void frambufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto w = reinterpret_cast<Window*> (glfwGetWindowUserPointer(window));
	w->_Resized(width, height);
}

Window::Window(uint32_t width, uint32_t height, const std::string& title):
m_width(width), m_height(height), m_title(title)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
	glfwSetWindowUserPointer(m_window, this);	
	glfwSetFramebufferSizeCallback(m_window, frambufferResizeCallback);

}

Window::~Window()
{
    glfwDestroyWindow(m_window);
    glfwTerminate();
}
