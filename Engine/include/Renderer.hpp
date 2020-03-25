#pragma once
#include <algorithm>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include "Window.hpp"
#include "CommandBuffer.hpp"
#include "Buffer.hpp"
#include "UniformBuffer.hpp"

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

	void RecreateSwapchain();
	void CleanupSwapchain();

	void CreateBuffers();
	void CreateUniformBuffers();
	void UpdateUniformBuffers(uint32_t currentImage);

	void CreateDescriptorSetLayout();
	void CreateDescriptorPool();
	void CreateDescriptorSets();

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

	std::unique_ptr<Buffer>  m_vertexBuffer;	
	std::unique_ptr<Buffer>  m_indexBuffer;

	VkDescriptorSetLayout	m_descriptorSetLayout;
	VkDescriptorPool		m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;
	std::vector<std::unique_ptr<UniformBuffer>> m_uniformBuffers;


	size_t m_currentFrame = 0;

	VkDebugUtilsMessengerEXT  m_messenger;

};
