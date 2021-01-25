#pragma once

#include "ECS/System.hpp"
#include "ECS/CoreEvents/ComponentEvents.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

#include "VulkanContext.hpp"
#include "Window.hpp"
#include "CommandBuffer.hpp"
#include "Buffer.hpp"
#include "DebugUI.hpp"
#include "UniformBuffer.hpp"
#include "Texture.hpp"
#include "ECS/CoreComponents/Mesh.hpp"
#include "Shader.hpp"


class RendererSystem : public System<RendererSystem>
{
public:
	RendererSystem(std::shared_ptr<Window> window);
	~RendererSystem();
	virtual	void Update(float dt) override;


private:
	void OnMeshComponentAdded(const ComponentAdded<Mesh>* e);
	void OnMeshComponentRemoved(const ComponentRemoved<Mesh>* e);

	// vulkan initialization stuff
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

	void CreateDescriptorSetLayout();
	void CreateDescriptorPool();
	void CreateDescriptorSets();

	void CreateTexture();
	void CreateSampler();

	void CreateColorResources();
	void CreateDepthResources();

	void SetupDebugMessenger();


	std::shared_ptr<Window> m_window;

	std::shared_ptr<DebugUI> m_debugUI;


	VkInstance				m_instance;
	VkPhysicalDevice&		m_gpu;
	VkDevice&				m_device;

	VkQueue					m_graphicsQueue;
	VkQueue					m_presentQueue;

	VkSurfaceKHR			m_surface;

	VkSwapchainKHR			m_swapchain;
	std::vector<VkImage>	m_swapchainImages;
	std::vector<VkImageView> m_swapchainImageViews;
	VkFormat				m_swapchainImageFormat;
	VkExtent2D				m_swapchainExtent;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;
	VkFramebuffer m_depthFramebuffers;

	VkRenderPass			m_renderPass;
	VkRenderPass			m_prePassRenderPass;

	struct PipelineInfo
	{
		VkPipelineLayout layout;
		VkPipeline pipeline;
		VkDescriptorSetLayout descriptorSetLayout;
		std::vector<Shader> shaders;
	};
	std::unordered_map<std::string, PipelineInfo> m_pipelines;
	UniformBuffer			m_cameraUBO;
	UniformBuffer			m_transformsUBO;

	VkPipelineLayout		m_pipelineLayout;
	VkPipeline				m_graphicsPipeline;

	VkPipelineLayout		m_prePassPipelineLayout;
	VkPipeline				m_prePassPipeline;

	VkCommandPool			m_commandPool;
	std::vector<CommandBuffer> m_depthCommandBuffers;
	std::vector<CommandBuffer> m_mainCommandBuffers;

	std::vector<VkSemaphore> m_prePassFinished;
	std::vector<VkSemaphore> m_imageAvailable;
	std::vector<VkSemaphore> m_renderFinished;
	std::vector<VkFence>     m_inFlightFences;
	std::vector<VkFence>     m_imagesInFlight;

	VkDescriptorSetLayout	m_descriptorSetLayout;
	VkDescriptorPool		m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;
	std::vector<std::unique_ptr<UniformBuffer>> m_uniformBuffers;

	VkSampler m_sampler; // TODO samplers are independent from the image (i think) so maybe we could use 1 sampler for multiple (every?) texture in the program
	std::unique_ptr<Texture> m_texture;

	std::unique_ptr<Image>   m_depthImage;


	VkSampleCountFlagBits   m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	std::unique_ptr<Image>  m_colorImage; // for MSAA

	size_t m_currentFrame = 0;

	VkDebugUtilsMessengerEXT  m_messenger;


	// one mesh has the same index in each of these vectors
	// TODO: maybe this should be in some kind of "static" component? like in the overwatch yt video
	// TODO: also maybe make the Buffer and CommandBuffer classes more cache friendly? not sure if needed tho
	std::vector<Buffer> m_vertexBuffers;
	std::vector<Buffer> m_indexBuffers;
	std::vector<std::vector<CommandBuffer>> m_commandBuffers;

};
