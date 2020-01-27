#include "main.h"
#include <iostream>
#include <Engine.hpp>

void run()
{
    std::cout << "Hello from run" << std::endl;
}
int main()
{
    std::shared_ptr<Window> window = std::make_shared<Window>(800, 600, "Vulkan Application");
    Renderer renderer(window);
    std::cout << "Hello from main" << std::endl;
    run();

    std::cin.get();
    return 0;
}