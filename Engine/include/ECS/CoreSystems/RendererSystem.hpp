#pragma once

#include "ECS/System.hpp"
#include "ECS/CoreEvents/ComponentEvents.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <set>

#include "VulkanContext.hpp"
#include "Window.hpp"
#include "CommandBuffer.hpp"
#include "Buffer.hpp"
#include "Utils/DebugUI.hpp"
#include "UniformBuffer.hpp"
#include "Texture.hpp"
#include "ECS/CoreComponents/Mesh.hpp"
#include "ECS/CoreComponents/Material.hpp"
#include "Shader.hpp"
#include "Memory/UniformBufferAllocator.hpp"
#include "Pipeline.hpp"
#include "RenderPass.hpp"
#include "Framebuffer.hpp"



class RendererSystem : public System<RendererSystem>
{
public:
	RendererSystem(std::shared_ptr<Window> window);
	~RendererSystem();
	virtual	void Update(float dt) override;

	Pipeline* AddPipeline(const std::string& name, PipelineCreateInfo createInfo, uint32_t priority);
	
	void AddDebugUIWindow(DebugUIWindow* window) { m_debugUI->AddWindow(window); };

private:
	friend class MaterialSystem;

	void OnMeshComponentAdded(const ComponentAdded<Mesh>* e);
	void OnMeshComponentRemoved(const ComponentRemoved<Mesh>* e);
	void OnMaterialComponentAdded(const ComponentAdded<Material>* e);

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


	VkInstance&				m_instance;
	VkPhysicalDevice&		m_gpu;
	VkDevice&				m_device;

	VkQueue&				m_graphicsQueue;
	VkQueue					m_presentQueue;

	VkSurfaceKHR			m_surface;

	VkSwapchainKHR			m_swapchain;
	std::vector<std::shared_ptr<Image>>	m_swapchainImages;
	VkFormat				m_swapchainImageFormat;
	VkExtent2D				m_swapchainExtent;

	RenderPass			m_renderPass;
	RenderPass			m_prePassRenderPass;

	std::multiset<Pipeline> m_pipelines;
	std::unordered_map<std::string, Pipeline*> m_pipelinesRegistry;

	std::unordered_map<std::string, std::unique_ptr<UniformBufferAllocator>> m_ubAllocators; //key: pipelineName + uboName(from shader) and "transforms" -> ub for storing the model matrices and "camera" -> VP matrix TODO: dont need one for the camera since its a global thing


	VkCommandPool&			m_commandPool;
	std::vector<CommandBuffer> m_mainCommandBuffers;

	std::vector<VkSemaphore> m_imageAvailable;
	std::vector<VkSemaphore> m_renderFinished;
	std::vector<VkFence>     m_inFlightFences;
	std::vector<VkFence>     m_imagesInFlight;

	VkDescriptorPool		m_descriptorPool;
	std::vector<VkDescriptorSet> m_cameraDescSets;
	std::vector<VkDescriptorSet> m_transformDescSets;
	// key: pipelineName + setNumber -> set 0, 2 = global(this set is stored in the above variables); set 1 = material;
	std::unordered_map<std::string, std::vector<VkDescriptorSet>> m_descriptorSets;

	std::shared_ptr<Image>   m_depthImage;

	VkSampleCountFlagBits   m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	std::shared_ptr<Image>  m_colorImage; // for MSAA

	size_t m_currentFrame = 0;

	VkDebugUtilsMessengerEXT  m_messenger;
};
