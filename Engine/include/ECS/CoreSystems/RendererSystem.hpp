#pragma once

#include "ECS/System.hpp"
#include "ECS/CoreEvents/ComponentEvents.hpp"

#include <vulkan/vulkan.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/shared_ptr.h>
#include <glm/glm.hpp>

#include "Window.hpp"
#include "CommandBuffer.hpp"
#include "Buffer.hpp"
#include "DebugUI.hpp"
#include "UniformBuffer.hpp"
#include "Texture.hpp"
#include "ECS/CoreComponents/Mesh.hpp"


class Renderer : public System<Renderer>
{
public:
	Renderer();
	~Renderer();
	virtual	void Update(float dt) override;

private:
	void OnMeshComponentAdded(const ComponentAdded<Mesh>* e);

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


	eastl::shared_ptr<Window> m_window;

	eastl::shared_ptr<DebugUI> m_debugUI;


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
	eastl::vector<CommandBuffer> m_mainCommandBuffers;

	eastl::vector<VkSemaphore> m_imageAvailable;
	eastl::vector<VkSemaphore> m_renderFinished;
	eastl::vector<VkFence>     m_inFlightFences;
	eastl::vector<VkFence>     m_imagesInFlight;

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


	// one mesh has the same index in each of these vectors
	// TODO: maybe this should be in some kind of "static" component? like in the overwatch yt video
	// TODO: also maybe make the Buffer and CommandBuffer classes more cache friendly? not sure if needed tho
	eastl::vector<Buffer> m_vertexBuffers;
	eastl::vector<Buffer> m_indexBuffers;
	eastl::vector<eastl::vector<CommandBuffer>> m_commandBuffers;

};
