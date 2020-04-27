#pragma once
#include <algorithm>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include "Window.hpp"
#include "CommandBuffer.hpp"
#include "Buffer.hpp"
#include "UniformBuffer.hpp"
#include "Texture.hpp"
#include "DebugUI.hpp"
#include "Model.hpp"
#include "Camera.hpp"

#include <glm/glm.hpp>
class Renderer
{
public:
	Renderer(eastl::shared_ptr<Window> window);
	~Renderer();

	void DrawFrame();

private:
	void CreateDebugUI();

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

	void CreateModel();
	void CreateUniformBuffers();
	void UpdateUniformBuffers(uint32_t currentImage);

	void CreateDescriptorSetLayout();
	void CreateDescriptorPool();
	void CreateDescriptorSets();

	void CreateTexture();
	void CreateSampler();

	void CreateColorResources();
	void CreateDepthResources();

	void SetupDebugMessenger();


	eastl::shared_ptr<Window> m_window;

	eastl::shared_ptr<DebugUI> m_debugUI;

	eastl::shared_ptr<Model> m_model;

	eastl::shared_ptr<Camera> m_camera;
	glm::mat4 m_vpMatrix;


	VkInstance				m_instance;
	VkPhysicalDevice		m_gpu;
	VkDevice				m_device;

	VkQueue					m_graphicsQueue;
	VkQueue					m_presentQueue;

	VkSurfaceKHR			m_surface;

	VkSwapchainKHR			m_swapchain;
	eastl::vector<VkImage>	m_swapchainImages;
	eastl::vector<VkImageView> m_swapchainImageViews;
	VkFormat				m_swapchainImageFormat;
	VkExtent2D				m_swapchainExtent;
	eastl::vector<VkFramebuffer> m_swapchainFramebuffers;

	VkRenderPass			m_renderPass;

	VkPipelineLayout		m_pipelineLayout;
	VkPipeline				m_graphicsPipeline;

	VkCommandPool			m_commandPool;

	eastl::vector<eastl::unique_ptr<CommandBuffer>> m_commandBuffers;

	eastl::vector<VkSemaphore> m_imageAvailable;
	eastl::vector<VkSemaphore> m_renderFinished;
	eastl::vector<VkFence>     m_inFlightFences;
	eastl::vector<VkFence>     m_imagesInFlight;

	eastl::unique_ptr<Buffer>  m_vertexBuffer;
	eastl::unique_ptr<Buffer>  m_indexBuffer;

	VkDescriptorSetLayout	m_descriptorSetLayout;
	VkDescriptorPool		m_descriptorPool;
	eastl::vector<VkDescriptorSet> m_descriptorSets;
	eastl::vector<eastl::unique_ptr<UniformBuffer>> m_uniformBuffers;

	VkSampler m_sampler; // TODO samplers are independent from the image (i think) so maybe we could use 1 sampler for multiple (every?) texture in the program
	eastl::unique_ptr<Texture> m_texture;

	eastl::unique_ptr<Image>   m_depthImage;


	VkSampleCountFlagBits   m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	eastl::unique_ptr<Image>  m_colorImage; // for MSAA

	size_t m_currentFrame = 0;

	VkDebugUtilsMessengerEXT  m_messenger;

	eastl::vector<eastl::unique_ptr<CommandBuffer>> m_cbs;
};
