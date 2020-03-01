#pragma once
#include <vulkan/vulkan.hpp>
#include <Window.hpp>

class Renderer
{
public:
    Renderer(std::shared_ptr<Window> window);
    ~Renderer();


private:
    void CreateInstance();
    void CreateSurface();
    void CreateDevice();
    void CreateSwapchain();
    void CreateRenderPass();
    void CreatePipeline();

    void SetupDebugMessenger();


    std::shared_ptr<Window> m_window;

    vk::Instance            m_instance;
    vk::PhysicalDevice      m_gpu;
    vk::Device              m_device;

    vk::Queue               m_graphicsQueue;
    vk::Queue               m_presentQueue;

    vk::SurfaceKHR          m_surface;

    vk::SwapchainKHR        m_swapchain;
    std::vector<vk::Image>  m_swapchainImages;
    std::vector<vk::ImageView> m_swapchainImageViews;
    vk::Format              m_swapchainImageFormat;
    vk::Extent2D            m_swapchainExtent;

    vk::RenderPass          m_renderPass;

    vk::DebugUtilsMessengerEXT  m_messenger;
};