#pragma once
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include "Window.hpp"
#include "CommandBuffer.hpp"

class Renderer
{
public:
    Renderer(std::shared_ptr<Window> window);
    ~Renderer();

	void DrawFrame();

private:
    void CreateInstance();
    void CreateSurface();
    void CreateDevice();
    void CreateSwapchain();
    void CreateRenderPass();
    void CreatePipeline();
	void CreateFramebuffers();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateSyncObjects();



    void SetupDebugMessenger();


    std::shared_ptr<Window> m_window;

    VkInstance				m_instance;
    VkPhysicalDevice		m_gpu;
    VkDevice				m_device;

    VkQueue					m_graphicsQueue;
    VkQueue					m_presentQueue;

    VkSurfaceKHR			m_surface;

    VkSwapchainKHR			m_swapchain;
    std::vector<VkImage>	m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkFormat				m_swapchainImageFormat;
    VkExtent2D				m_swapchainExtent;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;

    VkRenderPass			m_renderPass;

	VkPipelineLayout		m_pipelineLayout;
	VkPipeline				m_graphicsPipeline;

	VkCommandPool			m_commandPool;

	std::vector<std::unique_ptr<CommandBuffer>> m_commandBuffers;

	std::vector<VkSemaphore> m_imageAvailable;
	std::vector<VkSemaphore> m_renderFinished;
	std::vector<VkFence>     m_inFlightFences;
	std::vector<VkFence>     m_imagesInFlight;

	size_t m_currentFrame = 0;

	VkDebugUtilsMessengerEXT  m_messenger;

};
