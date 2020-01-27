#pragma once
#include <Windows.h> //needed otherwise vulkan.hpp throws error (its a bug in their code)
#include <vulkan/vulkan.hpp>
#include <Window.hpp>

class Renderer
{
public:
    Renderer(std::shared_ptr<Window> window);
    ~Renderer();


private:
    void CreateInstance();
    void SetupDebugMessenger();


    std::shared_ptr<Window> m_window;

    vk::Instance            m_instance;
    vk::PhysicalDevice      m_gpu;
    vk::Device              m_device;

    vk::DebugUtilsMessengerEXT  m_messenger;
};