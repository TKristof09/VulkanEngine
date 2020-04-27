#include "main.h"
#include <iostream>
#include <Engine.hpp>
void run()
{
    std::cout << "Hello from run" << std::endl;
}
int main()
{
    eastl::shared_ptr<Window> window = eastl::make_shared<Window>(1280, 720, "Vulkan Application");
    Renderer renderer(window);
    std::cout << "Hello from main" << std::endl;
    run();
    // TODO
    GLFWwindow* w = window->GetWindow();
    while(!glfwWindowShouldClose(w))
    {
        glfwPollEvents();
		renderer.DrawFrame();
    }
    glfwTerminate();
    std::cin.get();
    return 0;
}
