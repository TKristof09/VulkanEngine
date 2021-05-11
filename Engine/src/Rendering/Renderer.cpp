#include "Rendering/Renderer.hpp"
#include <sstream>
#include <optional>
#include <set>
#include <map>
#include <limits>
#include <chrono>
#include <array>
#include <string>
#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "TextureManager.hpp"
#include "ECS/ComponentManager.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "ECS/CoreComponents/Lights.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "ECS/CoreComponents/Renderable.hpp"
#include "ECS/CoreComponents/Material.hpp"

const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

VkPhysicalDevice VulkanContext::m_gpu = VK_NULL_HANDLE;
VkDevice VulkanContext::m_device = VK_NULL_HANDLE;
VkInstance VulkanContext::m_instance = VK_NULL_HANDLE;
VkPhysicalDeviceProperties VulkanContext::m_gpuProperties = {};
VkQueue VulkanContext::m_graphicsQueue = VK_NULL_HANDLE;
VkCommandPool VulkanContext::m_commandPool = VK_NULL_HANDLE;

struct UniformBufferObject
{
	glm::mat4 model;
};



struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentationFamily;

	bool IsComplete() {
		return graphicsFamily.has_value() && presentationFamily.has_value();
	}
};
struct SwapchainSupportDetails
{
	VkSurfaceCapabilitiesKHR            capabilities;
	std::vector<VkSurfaceFormatKHR>     formats;
	std::vector<VkPresentModeKHR>       presentModes;
};

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
std::vector<const char *> GetExtensions();
VkPhysicalDevice PickDevice(const std::vector<VkPhysicalDevice>& devices, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions);
int RateDevice(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions);
VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice device);
QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
SwapchainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
VkSurfaceFormatKHR ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
VkPresentModeKHR ChooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);

VkBool32 debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData, void * /*pUserData*/);




Renderer::Renderer(std::shared_ptr<Window> window, Scene* scene):
	m_ecs(scene->ecs),
	m_window(window),
	m_instance(VulkanContext::m_instance),
	m_gpu(VulkanContext::m_gpu),
	m_device(VulkanContext::m_device),
	m_graphicsQueue(VulkanContext::m_graphicsQueue),
	m_commandPool(VulkanContext::m_commandPool)
{
	
	
	scene->eventHandler->Subscribe(this, &Renderer::OnMeshComponentAdded);
	scene->eventHandler->Subscribe(this, &Renderer::OnMeshComponentRemoved);
	scene->eventHandler->Subscribe(this, &Renderer::OnMaterialComponentAdded);
	scene->eventHandler->Subscribe(this, &Renderer::OnDirectionalLightAdded);
	scene->eventHandler->Subscribe(this, &Renderer::OnPointLightAdded);
	scene->eventHandler->Subscribe(this, &Renderer::OnSpotLightAdded);

	CreateInstance();
	SetupDebugMessenger();
	CreateDevice();
	CreateSwapchain();

	m_computePushConstants.viewportSize = { m_swapchainExtent.width, m_swapchainExtent.height };
	m_computePushConstants.tileNums = glm::ceil(glm::vec2(m_computePushConstants.viewportSize) / 16.f);
	m_computePushConstants.lightNum = 0;
	m_newlyAddedLights.resize(m_swapchainImages.size());
	
	CreateRenderPass();
	CreatePipeline();
	CreateCommandPool();

	TextureManager::LoadTexture("./textures/error.jpg");
	
	CreateColorResources();
	CreateDepthResources();
	CreateFramebuffers();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();
	UpdateComputeDescriptors();
	CreateDebugUI();
	CreateCommandBuffers();
	CreateSyncObjects();

	

}

Renderer::~Renderer()
{
	vkDeviceWaitIdle(m_device);
	for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(m_device, m_imageAvailable[i], nullptr);
		vkDestroySemaphore(m_device, m_renderFinished[i], nullptr);
		vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
	}


	TextureManager::ClearLoadedTextures();

	CleanupSwapchain();

	m_debugUI.reset();

	m_ubAllocators.clear();
	m_pipelines.clear();
	m_depthPipeline.reset();

	m_lightsBuffers.clear();
	m_visibleLightsBuffers.clear();
	vkDestroySampler(m_device, m_computeSampler, nullptr);
	m_compute.reset();
	
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
	
	vkDestroyCommandPool(m_device, m_commandPool, nullptr);
	
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

	vkDestroyDevice(m_device, nullptr);

#ifdef VDEBUG
	DestroyDebugUtilsMessengerEXT(m_instance, m_messenger, nullptr);
#endif

	vkDestroyInstance(m_instance, nullptr);

}

void Renderer::RecreateSwapchain()
{
	while(m_window->GetWidth() == 0 || m_window->GetHeight() == 0)
		glfwWaitEvents();


	vkDeviceWaitIdle(m_device);

	CleanupSwapchain();

	vkDeviceWaitIdle(m_device);

	CreateSwapchain();
	CreateRenderPass();
	//CreatePipeline();
	CreateColorResources();
	CreateDepthResources();
	CreateFramebuffers();
	CreateCommandBuffers();
	UpdateComputeDescriptors();


	DebugUIInitInfo initInfo = {};
	initInfo.pWindow = m_window;


	QueueFamilyIndices families = FindQueueFamilies(m_gpu, m_surface);
	initInfo.queueFamily = families.graphicsFamily.value();
	initInfo.queue = m_graphicsQueue;

	initInfo.renderPass = &m_renderPass;
	initInfo.pipelineCache = nullptr;
	initInfo.descriptorPool = m_descriptorPool;
	initInfo.imageCount = static_cast<uint32_t>(m_swapchainImages.size());
	initInfo.msaaSamples = m_msaaSamples;
	initInfo.allocator = nullptr;
	m_debugUI->ReInit(initInfo);

	m_computePushConstants.viewportSize = { m_swapchainExtent.width, m_swapchainExtent.height };
	m_computePushConstants.tileNums = glm::ceil(glm::vec2(m_computePushConstants.viewportSize) / 16.f);
	
}

void Renderer::CleanupSwapchain()
{
	vkDeviceWaitIdle(m_device);
	
	m_depthImage.reset();
	m_colorImage.reset();


	m_mainCommandBuffers.clear();

	m_renderPass.Destroy();
	m_prePassRenderPass.Destroy();

	vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	for(auto& img : m_swapchainImages)
	{
		img->Free(true);
	}
	m_swapchainImages.clear();

}

void Renderer::CreateDebugUI()
{

	DebugUIInitInfo initInfo = {};
	initInfo.pWindow = m_window;

	QueueFamilyIndices families = FindQueueFamilies(m_gpu, m_surface);
	initInfo.queueFamily = families.graphicsFamily.value();
	initInfo.queue = m_graphicsQueue;

	initInfo.renderPass = &m_renderPass;
	initInfo.pipelineCache = nullptr;
	initInfo.descriptorPool = m_descriptorPool;
	initInfo.imageCount = static_cast<uint32_t>(m_swapchainImages.size());
	initInfo.msaaSamples = m_msaaSamples;
	initInfo.allocator = nullptr;


	m_debugUI = std::make_shared<DebugUI>(initInfo);
}

void Renderer::CreateInstance()
{
	VkApplicationInfo appinfo = {};
	appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appinfo.pApplicationName = "VulkanEngine";
	appinfo.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appinfo;

	auto extensions = GetExtensions();
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());

#ifdef VDEBUG


	// The VK_LAYER_KHRONOS_validation contains all current validation functionality.
	const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
	// Check if this layer is available at instance level
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> instanceLayerProperties(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, instanceLayerProperties.data());

	bool validationLayerPresent = false;
	LOG_TRACE("Available layers");
	for (VkLayerProperties layer : instanceLayerProperties) {
		LOG_TRACE("    {0}", layer.layerName);
		if (strcmp(layer.layerName, validationLayerName) == 0) {
			validationLayerPresent = true;
			break;
		}
	}
	if (validationLayerPresent) {
		createInfo.ppEnabledLayerNames = &validationLayerName;
		createInfo.enabledLayerCount = 1;
	} else {
		std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
	}


	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugCreateInfo.pfnUserCallback = debugUtilsMessengerCallback;
	
	createInfo.pNext = &debugCreateInfo;
#endif

	VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance), "Failed to create instance");

	VK_CHECK(glfwCreateWindowSurface(m_instance, m_window->GetWindow(), nullptr, &m_surface), "Failed ot create window surface!");

}

void Renderer::SetupDebugMessenger()
{
#ifndef VDEBUG
	return;
#endif
	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity =  VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugUtilsMessengerCallback;

	CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_messenger);
}

void Renderer::CreateDevice()
{
	std::vector<const char*> deviceExtensions;
	deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	deviceExtensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);

	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	m_gpu = PickDevice(devices, m_surface, deviceExtensions);

	vkGetPhysicalDeviceProperties(m_gpu, &VulkanContext::m_gpuProperties);
	LOG_TRACE("Picked device: {0}", VulkanContext::m_gpuProperties.deviceName);

	m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;//GetMaxUsableSampleCount(m_gpu);

	QueueFamilyIndices families = FindQueueFamilies(m_gpu, m_surface);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies          = {families.graphicsFamily.value(), families.presentationFamily.value()};

	for(auto queue : uniqueQueueFamilies)
	{

		float queuePriority                             = 1.0f;
		VkDeviceQueueCreateInfo queueCreateInfo			= {};
		queueCreateInfo.sType							= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex                = queue;
		queueCreateInfo.queueCount                      = 1;
		queueCreateInfo.pQueuePriorities                = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.sampleRateShading = VK_TRUE;
	//deviceFeatures.depthBounds = VK_TRUE; //doesnt work on my surface 2017
	// these are for dynamic descriptor indexing
	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {};
	descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
	descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
	descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
	descriptorIndexingFeatures.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;

	
	 

	
	VkDeviceCreateInfo createInfo		= {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.enabledExtensionCount    = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames  = deviceExtensions.data();
	createInfo.pEnabledFeatures         = &deviceFeatures;
	createInfo.pQueueCreateInfos        = queueCreateInfos.data();
	createInfo.queueCreateInfoCount     = static_cast<uint32_t>(queueCreateInfos.size());

	createInfo.pNext = &descriptorIndexingFeatures;

	VK_CHECK(vkCreateDevice(m_gpu, &createInfo, nullptr, &m_device), "Failed to create device");
	vkGetDeviceQueue(VulkanContext::m_device, families.graphicsFamily.value(), 0, &m_graphicsQueue);
	vkGetDeviceQueue(VulkanContext::m_device, families.presentationFamily.value(), 0, &m_presentQueue);


}

void Renderer::CreateSwapchain()
{
	SwapchainSupportDetails details = QuerySwapChainSupport(m_gpu, m_surface);

	auto surfaceFormat = ChooseSwapchainFormat(details.formats);
	auto extent = ChooseSwapchainExtent(details.capabilities, m_window->GetWindow());
	auto presentMode = ChooseSwapchainPresentMode(details.presentModes);

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType			= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface          = m_surface;
	createInfo.imageFormat      = surfaceFormat.format;
	createInfo.minImageCount    = details.capabilities.minImageCount + 1;
	createInfo.imageColorSpace  = surfaceFormat.colorSpace;
	createInfo.imageExtent      = extent;
	createInfo.presentMode      = presentMode;
	createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageArrayLayers = 1;

	QueueFamilyIndices indices        = FindQueueFamilies(m_gpu, m_surface);
	uint32_t queueFamilyIndices[]       = {indices.graphicsFamily.value(), indices.presentationFamily.value()};

	if (indices.graphicsFamily != indices.presentationFamily)
	{
		createInfo.imageSharingMode         = VK_SHARING_MODE_CONCURRENT; // if the two queues are different then we need to share between them
		createInfo.queueFamilyIndexCount    = 2;
		createInfo.pQueueFamilyIndices      = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE; // this is the better performance
		createInfo.queueFamilyIndexCount    = 0; // Optional
		createInfo.pQueueFamilyIndices      = nullptr; // Optional
	}
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.preTransform = details.capabilities.currentTransform;
	createInfo.clipped      = VK_TRUE;

	VK_CHECK(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain), "Failed to create swapchain");

	uint32_t imageCount;
	VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr), "Failed to get swapchain images");
	std::vector<VkImage> tempImages(imageCount);
	VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, tempImages.data()), "Failed to get swapchain images");
	m_swapchainExtent = extent;
	m_swapchainImageFormat = surfaceFormat.format;

	for (size_t i = 0; i < tempImages.size(); i++)
	{
		ImageCreateInfo ci = {};
		ci.image = tempImages[i];
		ci.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		ci.format = m_swapchainImageFormat;

		m_swapchainImages.emplace_back(std::make_shared<Image>(extent, ci));
	}

	LOG_TRACE("Created swapchain and images");
}

void Renderer::CreateRenderPass()
{

	// Main render pass
	RenderPassAttachment colorAttachment = {};
	colorAttachment.type = RenderPassAttachmentType::COLOR;
	colorAttachment.format = m_swapchainImageFormat;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if(m_msaaSamples != VK_SAMPLE_COUNT_1_BIT)
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	else
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	colorAttachment.internalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.clearValue.color = {0.1f, 0.1f, 0.1f, 1.0f};


	RenderPassAttachment depthAttachment = {};
	depthAttachment.type = RenderPassAttachmentType::DEPTH;
	depthAttachment.format = VK_FORMAT_D32_SFLOAT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	depthAttachment.internalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	RenderPassAttachment colorAttachmentResolve = {}; // To "combine" all the samples from the msaa attachment
	colorAttachmentResolve.type = RenderPassAttachmentType::RESOLVE;
	colorAttachmentResolve.format = m_swapchainImageFormat;
	colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	colorAttachmentResolve.internalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	RenderPassCreateInfo ci;
	ci.attachments = {colorAttachment, depthAttachment };
	ci.msaaSamples = m_msaaSamples;
	ci.subpassCount = 1;
	ci.dependencies = { dependency };

	m_renderPass = RenderPass(ci);
	LOG_TRACE("Created main render pass");


	// Render pass for the depth prepass
	RenderPassAttachment prePassDepthAttachment = {};
	prePassDepthAttachment.type = RenderPassAttachmentType::DEPTH;
	prePassDepthAttachment.format = VK_FORMAT_D32_SFLOAT;
	prePassDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	prePassDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	prePassDepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	prePassDepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	prePassDepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	prePassDepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	prePassDepthAttachment.internalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	prePassDepthAttachment.clearValue.depthStencil = {1.0f, 0 };


	std::vector<VkSubpassDependency> dependencies(2);

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	
	RenderPassCreateInfo prepassCi;
	prepassCi.attachments = {prePassDepthAttachment};
	prepassCi.msaaSamples = m_msaaSamples;
	prepassCi.subpassCount = 1;
	prepassCi.dependencies = dependencies;

	m_prePassRenderPass = RenderPass(prepassCi);
	LOG_TRACE("Created prepass render pass");

}

Pipeline* Renderer::AddPipeline(const std::string& name, PipelineCreateInfo createInfo, uint32_t priority)
{
	LOG_TRACE("Creating pipeline for {0}", name);

	createInfo.renderPass = &m_renderPass;
	createInfo.viewportExtent = m_swapchainExtent;
	createInfo.msaaSamples = m_msaaSamples;
	auto it = m_pipelines.emplace(name, createInfo, priority);
	Pipeline* pipeline = const_cast<Pipeline*>(&(*it));
	m_pipelinesRegistry[name] = pipeline;
	for(uint32_t i = 0; i < m_swapchainImages.size(); ++i)
	{
		for(auto& [name, bufferInfo] : pipeline->m_uniformBuffers)
		{
			m_ubAllocators[pipeline->m_name + name + std::to_string(i)] = std::move(std::make_unique<BufferAllocator>(bufferInfo.size, OBJECTS_PER_DESCRIPTOR_CHUNK, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
		}
	}

	return pipeline;
}
void Renderer::CreatePipeline()
{
	// Compute
	PipelineCreateInfo compute = {};
	compute.type = PipelineType::COMPUTE;
	m_compute = std::make_unique<Pipeline>("lightCulling", compute);

	VkSamplerCreateInfo ci = {};
	ci.sType	= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	ci.magFilter = VK_FILTER_LINEAR;
	ci.minFilter = VK_FILTER_LINEAR;
	ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	ci.anisotropyEnable = VK_TRUE;
	ci.maxAnisotropy = 1.0f;
	ci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	ci.unnormalizedCoordinates = VK_FALSE; // this should always be false because UV coords are in [0,1) not in [0, width),etc...
	ci.compareEnable = VK_FALSE; // this is used for percentage closer filtering for shadow maps
	ci.compareOp     = VK_COMPARE_OP_ALWAYS;
	ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	ci.mipLodBias = 0.0f;
	ci.minLod = 0.0f;
	ci.maxLod = 1.0f;

	VK_CHECK(vkCreateSampler(VulkanContext::GetDevice(), &ci, nullptr, &m_computeSampler), "Failed to create depth texture sampler");

	
	// Depth prepass pipeline
	LOG_WARN("Creating depth pipeline");
	PipelineCreateInfo depthPipeline;
	depthPipeline.type				= PipelineType::GRAPHICS;
	//depthPipeline.parent			= const_cast<Pipeline*>(&(*mainIter));
	depthPipeline.depthCompareOp	= VK_COMPARE_OP_LESS;
	depthPipeline.depthWriteEnable	= true;
	depthPipeline.useDepth			= true;
	depthPipeline.depthClampEnable  = false;
	depthPipeline.msaaSamples		= m_msaaSamples;
	depthPipeline.renderPass		= &m_prePassRenderPass;
	depthPipeline.subpass			= 0;
	depthPipeline.stages			= VK_SHADER_STAGE_VERTEX_BIT;
	depthPipeline.useMultiSampling	= true;
	depthPipeline.useColorBlend		= false;
	depthPipeline.viewportExtent	= m_swapchainExtent;

	depthPipeline.isGlobal			= true;

	m_depthPipeline = std::make_unique<Pipeline>("depth", depthPipeline, 0);
	//auto depthIter = m_pipelines.emplace("depth", depthPipeline, 0);

	//Pipeline* pipeline = const_cast<Pipeline*>(&(*depthIter));
	//m_pipelinesRegistry["depth"] = pipeline;

	uint32_t totaltiles = glm::compMul(m_computePushConstants.tileNums);
	
	for(uint32_t i = 0; i < m_swapchainImages.size(); ++i)
	{

		for(auto& [name, bufferInfo] : m_depthPipeline->m_uniformBuffers)
		{
			if(m_ubAllocators[m_depthPipeline->m_name + name + std::to_string(i)] == nullptr)
				m_ubAllocators[m_depthPipeline->m_name + name + std::to_string(i)] = std::move(std::make_unique<BufferAllocator>(bufferInfo.size, OBJECTS_PER_DESCRIPTOR_CHUNK, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
		}

		// TODO
		m_lightsBuffers.emplace_back(std::make_unique<BufferAllocator>(sizeof(Light), 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)); // MAX_LIGHTS_PER_TILE
		m_visibleLightsBuffers.emplace_back(std::make_unique<BufferAllocator>(sizeof(TileLights), totaltiles, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)); // MAX_LIGHTS_PER_TILE
	
	}

}

void Renderer::CreateFramebuffers()
{
	for (size_t i = 0; i < m_swapchainImages.size(); i++)
	{

		FramebufferCreateInfo ci = {};
		ci.width = m_swapchainExtent.width;
		ci.height = m_swapchainExtent.height;
		if(m_msaaSamples != VK_SAMPLE_COUNT_1_BIT)
		{

			ci.attachmentDescriptions.push_back({ m_colorImage, {}});
			ci.attachmentDescriptions.push_back({ m_depthImage, {}});
			ci.attachmentDescriptions.push_back({ m_swapchainImages[i], {}});
		}
		else
		{
			ci.attachmentDescriptions.push_back({ m_swapchainImages[i], {}});
			ci.attachmentDescriptions.push_back({ m_depthImage, {}});
		}

		m_renderPass.AddFramebuffer(ci);
		ci = {};
		ci.width = m_swapchainExtent.width;
		ci.height = m_swapchainExtent.height;
		ci.attachmentDescriptions = { { m_depthImage, {}} };
		m_prePassRenderPass.AddFramebuffer(ci);
	}
}



void Renderer::CreateCommandPool()
{
	QueueFamilyIndices families = FindQueueFamilies(m_gpu, m_surface);
	VkCommandPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = families.graphicsFamily.value();
	createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VK_CHECK(vkCreateCommandPool(m_device, &createInfo, nullptr, &m_commandPool), "Failed to create command pool");
}

void Renderer::CreateCommandBuffers()
{
	for(size_t i = 0; i < m_swapchainImages.size(); i++)
	{
		m_mainCommandBuffers.push_back(CommandBuffer());
	}
}

void Renderer::CreateSyncObjects()
{
	m_imageAvailable.resize(MAX_FRAMES_IN_FLIGHT);
	m_renderFinished.resize(MAX_FRAMES_IN_FLIGHT);
	m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	m_imagesInFlight.resize(m_swapchainImages.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{

		VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailable[i]), "Failed to create semaphore");
		VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinished[i]), "Failed to create semaphore");

		VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]), "Failed to create fence");
	}

}

void Renderer::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 5> poolSizes   = {};
	poolSizes[0].type                               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount                    = static_cast<uint32_t>(m_swapchainImages.size());
	poolSizes[1].type                               = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount                    = 10000; // +1 for imgui
	poolSizes[2].type								= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	poolSizes[2].descriptorCount					= 100; //static_cast<uint32_t>(m_swapchainImages.size());;
	poolSizes[3].type								= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[3].descriptorCount					= 100; //static_cast<uint32_t>(m_swapchainImages.size());;
	poolSizes[4].type								= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[4].descriptorCount					= 100; //static_cast<uint32_t>(m_swapchainImages.size());;


	VkDescriptorPoolCreateInfo createInfo   = {};
	createInfo.sType                        = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.poolSizeCount                = static_cast<uint32_t>(poolSizes.size());
	createInfo.pPoolSizes                   = poolSizes.data();
	createInfo.maxSets                      = 5000;
	VK_CHECK(vkCreateDescriptorPool(m_device, &createInfo, nullptr, &m_descriptorPool), "Failed to create descriptor pool");
}

void Renderer::CreateDescriptorSets()
{

	
	VkDescriptorSetAllocateInfo allocInfo = {};
	std::vector<VkDescriptorSetLayout> layouts(m_swapchainImages.size());

	for (int i = 0; i < m_swapchainImages.size(); ++i)
	{
		layouts[i] = m_depthPipeline->m_descSetLayouts[1];
	}
	allocInfo = {};
	allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool		= m_descriptorPool;
	allocInfo.descriptorSetCount	= static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts			= layouts.data();
	m_descriptorSets[m_depthPipeline->m_name + std::to_string(1)].resize(m_swapchainImages.size());
	VK_CHECK(vkAllocateDescriptorSets(VulkanContext::GetDevice(), &allocInfo, m_descriptorSets[m_depthPipeline->m_name + std::to_string(1)].data()), "Failed to allocate descriptor sets");



	std::vector<VkDescriptorSetLayout> cameraLayouts(m_swapchainImages.size(), m_depthPipeline->m_descSetLayouts[0]); // TODO 0 is the global for now, maybe make it so that each pipeline doesnt store a copy of this layout

	allocInfo = {};
	allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool		= m_descriptorPool;
	allocInfo.descriptorSetCount	= static_cast<uint32_t>(cameraLayouts.size());
	allocInfo.pSetLayouts			= cameraLayouts.data();

	m_cameraDescSets.resize(m_swapchainImages.size());
	VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, m_cameraDescSets.data()), "Failed to allocate global descriptor sets");




	// TODO TEMP
	std::vector<VkDescriptorSetLayout> depthLayouts(m_swapchainImages.size(), m_depthPipeline->m_descSetLayouts[1]); // TODO 0 is the global for now, maybe make it so that each pipeline doesnt store a copy of this layout

	allocInfo = {};
	allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool		= m_descriptorPool;
	allocInfo.descriptorSetCount	= static_cast<uint32_t>(depthLayouts.size());
	allocInfo.pSetLayouts			= depthLayouts.data();

	m_descriptorSets["depth1"].resize(m_swapchainImages.size());
	VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets["depth1"].data()), "Failed to allocate global descriptor sets");

	for(uint32_t i=0; i < m_swapchainImages.size(); ++i)
	{
		VkDescriptorBufferInfo bufferI = {};
		bufferI.buffer	= m_ubAllocators["depthFiller" + std::to_string(i)]->GetBuffer(0);
		bufferI.offset	= 0;
		bufferI.range	= m_ubAllocators["depthFiller" + std::to_string(i)]->GetObjSize();

		VkWriteDescriptorSet writeDS = {};
		writeDS.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDS.dstSet			= m_descriptorSets["depth1"][i];
		writeDS.dstBinding		= 0;
		writeDS.descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		writeDS.descriptorCount	= 1;
		writeDS.pBufferInfo		= &bufferI;

		vkUpdateDescriptorSets(m_device, 1, &writeDS, 0, nullptr);




		bufferI = {};
		bufferI.buffer	= m_ubAllocators["camera" + std::to_string(i)]->GetBuffer(0);
		bufferI.offset	= 0;
		bufferI.range	= m_ubAllocators["camera" + std::to_string(i)]->GetObjSize();

		writeDS = {};
		writeDS.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDS.dstSet			= m_cameraDescSets[i];
		writeDS.dstBinding		= 0;
		writeDS.descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		writeDS.descriptorCount	= 1;
		writeDS.pBufferInfo		= &bufferI;

		vkUpdateDescriptorSets(m_device, 1, &writeDS, 0, nullptr);

	}


	
	VkDescriptorSetAllocateInfo callocInfo = {};
	std::vector<VkDescriptorSetLayout> clayouts;

	if(m_computeDesc.size() != 0)
		return;
	for (int i = 0; i < m_swapchainImages.size(); ++i)
	{
		for(int j = 0; j < m_compute->m_numDescSets; ++j)
			clayouts.push_back(m_compute->m_descSetLayouts[j]);
	}
	callocInfo.sType				= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	callocInfo.descriptorPool		= m_descriptorPool;
	callocInfo.descriptorSetCount	= static_cast<uint32_t>(clayouts.size());
	callocInfo.pSetLayouts			= clayouts.data();
	m_computeDesc.resize(clayouts.size());
	VK_CHECK(vkAllocateDescriptorSets(VulkanContext::GetDevice(), &callocInfo, m_computeDesc.data()), "Failed to allocate descriptor sets");

	


}

void Renderer::CreateUniformBuffers()
{
	for(uint32_t i=0; i < m_swapchainImages.size(); ++i)
	{
		m_ubAllocators["transforms" + std::to_string(i)] = std::move(std::make_unique<BufferAllocator>(sizeof(glm::mat4), OBJECTS_PER_DESCRIPTOR_CHUNK, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
		m_ubAllocators["camera" + std::to_string(i)] = std::move(std::make_unique<BufferAllocator>(sizeof(CameraStruct), OBJECTS_PER_DESCRIPTOR_CHUNK, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));

		m_ubAllocators["camera" + std::to_string(i)]->Allocate();
	}

}

void Renderer::CreateColorResources()
{
	ImageCreateInfo ci;
	ci.format = m_swapchainImageFormat;
	ci.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	ci.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	ci.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	ci.msaaSamples = m_msaaSamples;
	ci.useMips = false;
	m_colorImage = std::make_shared<Image>(m_swapchainExtent, ci);
}

void Renderer::CreateDepthResources()
{
	ImageCreateInfo ci;
	ci.format = VK_FORMAT_D32_SFLOAT;
	ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	ci.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	ci.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	ci.msaaSamples = m_msaaSamples;
	ci.useMips = false;
	m_depthImage = std::make_shared<Image>(m_swapchainExtent, ci);

}

void Renderer::UpdateComputeDescriptors()
{
	TextureManager::LoadTexture("./textures/texture.jpg");
	Texture& t = TextureManager::GetTexture("./textures/texture.jpg");
	for(uint32_t i=0; i < m_swapchainImages.size(); ++i)
	{
		VkDescriptorBufferInfo cameraBI = {};
		cameraBI.offset = 0;
		cameraBI.range	= m_ubAllocators["camera" + std::to_string(i)]->GetObjSize();
		cameraBI.buffer = m_ubAllocators["camera" + std::to_string(i)]->GetBuffer(0);
		
		VkDescriptorBufferInfo lightsBI = {};
		lightsBI.offset	= 0;
		lightsBI.range	= VK_WHOLE_SIZE;
		lightsBI.buffer	= m_lightsBuffers[i]->GetBuffer(0);
		
		VkDescriptorBufferInfo visibleLightsBI = {};
		visibleLightsBI.offset	= 0;
		visibleLightsBI.range	= VK_WHOLE_SIZE;
		visibleLightsBI.buffer	= m_visibleLightsBuffers[i]->GetBuffer(0);

		std::array<VkDescriptorBufferInfo, 2> bufferInfos = {lightsBI, visibleLightsBI};
		
		VkDescriptorImageInfo depthImageInfo = {};
		depthImageInfo.imageLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		depthImageInfo.imageView	= m_depthImage->GetImageView();
		depthImageInfo.sampler		= m_computeSampler;

		std::array<VkWriteDescriptorSet, 3> writeDS({});
		writeDS[0].sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDS[0].dstSet			= m_computeDesc[i];
		writeDS[0].dstBinding		= 0;
		writeDS[0].descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		writeDS[0].descriptorCount	= 1;
		writeDS[0].pBufferInfo		= &cameraBI;
		
		writeDS[1].sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDS[1].dstSet			= m_computeDesc[i];
		writeDS[1].dstBinding		= 1;
		writeDS[1].descriptorType	= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDS[1].descriptorCount	= bufferInfos.size();
		writeDS[1].pBufferInfo		= bufferInfos.data();
		
		writeDS[2].sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDS[2].dstSet			= m_computeDesc[i];
		writeDS[2].dstBinding		= 3;
		writeDS[2].descriptorType	= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDS[2].descriptorCount	= 1;
		writeDS[2].pImageInfo		= &depthImageInfo;
		
		
		vkUpdateDescriptorSets(m_device, writeDS.size(), writeDS.data(), 0, nullptr);
	}
}

void Renderer::UpdateLights(uint32_t index)
{
	std::vector<std::pair<uint32_t, void*>> slotsAndDatas;
	
	for(auto* lightComp : m_ecs->componentManager->GetComponents<DirectionalLight>())
	{
		Transform* t = m_ecs->componentManager->GetComponent<Transform>(lightComp->GetOwner());
		glm::mat4 tMat = t->GetTransform();
		
		glm::vec3 newDir = glm::normalize(glm::vec3(tMat[2]));
		float newIntensity = lightComp->intensity;
		glm::vec3 newColor = lightComp->color.ToVec3();


		Light& light = m_lightMap[lightComp->GetComponentID()];
		bool changed = newDir != light.direction || newIntensity != light.intensity || newColor != light.color;
		if(changed)
		{
			
			light.direction = newDir;
			light.intensity = newIntensity;
			light.color = newColor;
			
			slotsAndDatas.push_back({lightComp->_slot, &light});
		}
	}
	for(auto* lightComp : m_ecs->componentManager->GetComponents<PointLight>())
	{
		Transform* t = m_ecs->componentManager->GetComponent<Transform>(lightComp->GetOwner());
		glm::mat4 tMat = t->GetTransform();

		glm::vec3 newPos = glm::vec3(tMat[3]);
		float newRange = lightComp->range;
		float newIntensity = lightComp->intensity;
		glm::vec3 newColor = lightComp->color.ToVec3();
		Attenuation newAtt = lightComp->attenuation;

		Light& light = m_lightMap[lightComp->GetComponentID()];
		bool changed = newPos != light.position || newIntensity != light.intensity
					|| newColor != light.color || newRange != light.range
					|| newAtt.quadratic != light.attenuation[0] || newAtt.linear != light.attenuation[1] || newAtt.constant != light.attenuation[2];
		if(changed)
		{
			light.position = newPos;
			light.range = newRange;
			light.intensity = newIntensity;
			light.color = newColor;
			
			light.attenuation[0] = newAtt.quadratic;
			light.attenuation[1] = newAtt.linear;
			light.attenuation[2] = newAtt.constant;
			
			slotsAndDatas.push_back({lightComp->_slot, &light});
		}
	}
	for(auto* lightComp : m_ecs->componentManager->GetComponents<SpotLight>())
	{
		Transform* t = m_ecs->componentManager->GetComponent<Transform>(lightComp->GetOwner());
		glm::mat4 tMat = t->GetTransform();
		
		glm::vec3 newPos = glm::vec3(tMat[3]);
		glm::vec3 newDir = glm::normalize(glm::vec3(tMat[2]));
		float newRange = lightComp->range;
		float newIntensity = lightComp->intensity;
		float newCutoff = lightComp->cutoff;
		glm::vec3 newColor = lightComp->color.ToVec3();

		Attenuation newAtt = lightComp->attenuation;


		Light& light = m_lightMap[lightComp->GetComponentID()];
		bool changed = newPos != light.position || newDir != light.direction|| newIntensity != light.intensity
				|| newColor != light.color || newRange != light.range || newCutoff != light.cutoff
				|| newAtt.quadratic != light.attenuation[0] || newAtt.linear != light.attenuation[1] || newAtt.constant != light.attenuation[2];
		
		if(changed)
		{
			light.position = newPos;
			light.direction = newDir;
			light.range = newRange;
			light.intensity = newIntensity;
			light.cutoff = newCutoff;
			light.color = newColor;
			
			light.attenuation[0] = newAtt.quadratic;
			light.attenuation[1] = newAtt.linear;
			light.attenuation[2] = newAtt.constant;
			
			slotsAndDatas.push_back({lightComp->_slot, &light});
		}
	}

	for(auto& [slot, newLight] : m_newlyAddedLights[index])
	{
		slotsAndDatas.push_back({slot, newLight});
	}
	m_newlyAddedLights[index].clear();
	
	m_lightsBuffers[index]->UpdateBuffer(slotsAndDatas);
	
}


void Renderer::Render(double dt)
{
	vkDeviceWaitIdle(m_device);	
	vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &imageIndex);


	if(result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapchain();
		return;
	}
	else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		throw std::runtime_error("Failed to acquire swap chain image");

	// Check if a previous frame is using this image (i.e. there is its fence to wait on)
	if(m_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
		vkWaitForFences(m_device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
	// Mark the image as now being in use by this frame
	m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];


	VK_CHECK(vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]), "Failed to reset in flight fences");


	UpdateLights(imageIndex);

	//m_debugUI->SetupFrame(imageIndex, 0, &m_renderPass);	//subpass is 0 because we only have one subpass for now

	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();



	int i = -1;
	ComponentManager* cm = m_ecs->componentManager;


	// TODO: make something better for the cameras, now it only takes the first camera that was added to the componentManager
	// also a single camera has a whole chunk of memory which isnt good if we only use 1 camera per game
	Camera camera = *(*(cm->begin<Camera>()));
	Transform* cameraTransform = cm->GetComponent<Transform>(camera.GetOwner());
	auto materialIterator = cm->begin<Material>();
	auto end = cm->end<Material>();
	EntityID currentEntity = materialIterator->GetOwner();

	RenderPass* lastRenderPass = nullptr;
	m_mainCommandBuffers[imageIndex].Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VkCommandBuffer cb = m_mainCommandBuffers[imageIndex].GetCommandBuffer();
	


	
	glm::mat4 proj = camera.GetProjection();
	//glm::mat4 trf = glm::translate(-cameraTransform->wPosition);
	//glm::mat4 rot = glm::toMat4(glm::conjugate(cameraTransform->wRotation));
	//glm::mat4 vp = proj * rot * trf;
	glm::mat4 vp = proj * glm::inverse(cameraTransform->GetTransform()); //TODO inversing the transform like this isnt too fast, consider only allowing camera on root level entity so we can just -pos and -rot
	uint32_t vpOffset = 0;
	
	CameraStruct cs = {vp, cameraTransform->pos};
	m_ubAllocators["camera" + std::to_string(imageIndex)]->UpdateBuffer(0, &cs);

	auto prepassBegin = m_depthPipeline->m_renderPass->GetBeginInfo(imageIndex);
	vkCmdBeginRenderPass(cb, &prepassBegin, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthPipeline->m_pipeline);

	// This should be fine since all our shaders use set 0 for the camera so the descriptor doesn't get unbound by pipeline switches
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthPipeline->m_layout, 0, 1, &m_cameraDescSets[imageIndex], 1, &vpOffset);

	for(auto* renderable : cm->GetComponents<Renderable>())
	{
		auto transform  = cm->GetComponent<Transform>(renderable->GetOwner());
		glm::mat4 model =  transform->GetTransform();
		renderable->vertexBuffer.Bind(m_mainCommandBuffers[imageIndex]);
		renderable->indexBuffer.Bind(m_mainCommandBuffers[imageIndex]);

		uint32_t offset;
		uint32_t bufferIndex = m_ubAllocators["transforms" + std::to_string(imageIndex)]->GetBufferIndexAndOffset(transform->ub_id, offset);

		m_ubAllocators["transforms" + std::to_string(imageIndex)]->UpdateBuffer(transform->ub_id, &model);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthPipeline->m_layout, 2, 1, &m_transformDescSets[bufferIndex + imageIndex], 1, &offset);
		vkCmdDrawIndexed(cb, static_cast<uint32_t>(renderable->indexBuffer.GetSize() / sizeof(uint32_t)) , 1, 0, 0, 0); // TODO: make the size calculation better (not hardcoded for uint32_t)
	}
	vkCmdEndRenderPass(cb);
	
	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	// We won't be changing the layout of the image
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	imageMemoryBarrier.image = m_depthImage->GetImage();
	imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkCmdPipelineBarrier(
		cb,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier);
	uint32_t offset = 0;
	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute->m_pipeline);
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute->m_layout, 0, 1, &m_computeDesc[imageIndex], 1, &offset);
	
	
	vkCmdPushConstants(cb, m_compute->m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &m_computePushConstants);
	vkCmdDispatch(cb, m_computePushConstants.tileNums.x, m_computePushConstants.tileNums.y, 1);
	
	for(auto& pipeline : m_pipelines)
	{
		//if(pipeline.m_name == "depth") continue;
		auto tempIt = materialIterator;// to be able to restore the iterator to its previous place after global pipeline is finished
		if(pipeline.m_isGlobal)
			materialIterator = cm->begin<Material>();

		if(pipeline.m_renderPass != lastRenderPass)
		{
			if(lastRenderPass)
				vkCmdEndRenderPass(cb);

			lastRenderPass = pipeline.m_renderPass;
			auto beginInfo = lastRenderPass->GetBeginInfo(imageIndex);
			vkCmdBeginRenderPass(cb, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
			
			//vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_layout, 0, 1, m_descriptorSets[pipeline.m_name + "0"][imageIndex], __, __); TODO use a global descriptor
			
		}


		//vkCmdPushConstants(m_mainCommandBuffers[imageIndex].GetCommandBuffer(), pipeline.m_layout,
		
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_pipeline);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_layout, 0, 1, &m_tempDesc[imageIndex], 1, &offset);
		vkCmdPushConstants(cb, pipeline.m_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ComputePushConstants), &m_computePushConstants);
		

		while(materialIterator != end && (pipeline.m_isGlobal || materialIterator->shaderName == pipeline.m_name))
		{
			Transform* transform = cm->GetComponent<Transform>(currentEntity);
			Renderable* renderable = cm->GetComponent<Renderable>(currentEntity);
			if(renderable == nullptr)
				continue;

			// We don't need to update the transforms here, because they have already been updated in the depth prepass draw loop
			
			//transform->rot = glm::rotate(transform->rot,  i * 0.001f * glm::radians(90.0f), glm::vec3(0,0,1));
			//glm::mat4 model =  transform->GetTransform();

			renderable->vertexBuffer.Bind(m_mainCommandBuffers[imageIndex]);
			renderable->indexBuffer.Bind(m_mainCommandBuffers[imageIndex]);

			uint32_t offset;
			uint32_t bufferIndex = m_ubAllocators["transforms" + std::to_string(imageIndex)]->GetBufferIndexAndOffset(transform->ub_id, offset);

			//m_ubAllocators["transforms" + std::to_string(imageIndex)]->UpdateBuffer(transform->ub_id, &model);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_layout, 2, 1, &m_transformDescSets[bufferIndex + imageIndex], 1, &offset);


			if(!pipeline.m_isGlobal)
			{
				m_ubAllocators[pipeline.m_name + "material" + std::to_string(imageIndex)]->GetBufferIndexAndOffset(materialIterator->_ubSlot, offset);

				glm::vec4 ubs[] = {{std::sin(time) * 0.5f + 0.5f, 0,0,1.0f}, {materialIterator->_textureSlot, 0, 0, 0}};

				m_ubAllocators[pipeline.m_name + "material" + std::to_string(imageIndex)]->UpdateBuffer(materialIterator->_ubSlot, ubs);

			}
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_layout, 1, 1, &m_descriptorSets[pipeline.m_name + std::to_string(1)][bufferIndex + imageIndex], 1, &offset);
			vkCmdDrawIndexed(cb, static_cast<uint32_t>(renderable->indexBuffer.GetSize() / sizeof(uint32_t)) , 1, 0, 0, 0); // TODO: make the size calculation better (not hardcoded for uint32_t)


			i *= -1;
			materialIterator++;
			if(materialIterator != end)
				currentEntity = materialIterator->GetOwner();
		}
		if(pipeline.m_isGlobal && tempIt != end)
		{
			materialIterator = tempIt;
			currentEntity = materialIterator->GetOwner();
		}
	}



	m_debugUI->Draw(&m_mainCommandBuffers[imageIndex]);


	vkCmdEndRenderPass(cb);
	m_mainCommandBuffers[imageIndex].End();

	VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cb;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_imageAvailable[m_currentFrame];
	submitInfo.pWaitDstStageMask = &wait;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_renderFinished[m_currentFrame];

	VK_CHECK(vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]), "Failed to reset fence");
	VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]), "Failed to submit command buffers");


	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderFinished[m_currentFrame];

	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain;
	presentInfo.pImageIndices = &imageIndex;
	result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

	if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window->IsResized())
	{
		m_window->SetResized(false);
		RecreateSwapchain();
	}
	else if(result != VK_SUCCESS)
		throw std::runtime_error("Failed to present swap chain image");
	m_currentFrame = (m_currentFrame +1 ) % MAX_FRAMES_IN_FLIGHT;

}


void Renderer::OnMeshComponentAdded(const ComponentAdded<Mesh>* e)
{
	Mesh* mesh = e->component;
	VkDeviceSize vBufferSize = sizeof(mesh->vertices[0]) * mesh->vertices.size();

	// create the vertex and index buffers on the gpu
	Buffer stagingVertexBuffer(vBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	stagingVertexBuffer.Fill((void*)mesh->vertices.data(), vBufferSize);

	VkDeviceSize iBufferSize = sizeof(mesh->indices[0]) * mesh->indices.size();
	Buffer stagingIndexBuffer(iBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	stagingIndexBuffer.Fill((void*)mesh->indices.data(), iBufferSize);


	Renderable* comp = m_ecs->componentManager->AddComponent<Renderable>(e->entity);
	comp->vertexBuffer = Buffer(vBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	comp->indexBuffer = Buffer(iBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	stagingVertexBuffer.Copy(&comp->vertexBuffer, vBufferSize);

	stagingIndexBuffer.Copy(&comp->indexBuffer, iBufferSize);

	stagingVertexBuffer.Free();
	stagingIndexBuffer.Free();

	uint32_t id;
	for(uint32_t i = 0; i < m_swapchainImages.size(); ++i)
	{
		id = m_ubAllocators["transforms" + std::to_string(i)]->Allocate();
		m_ecs->componentManager->GetComponent<Transform>(e->entity)->ub_id = id;

	}

	uint32_t offset;
	uint32_t bufferIndex = m_ubAllocators["transforms" + std::to_string(0)]->GetBufferIndexAndOffset(id, offset);
	if(bufferIndex >= m_transformDescSets.size())
	{
		std::vector<VkDescriptorSetLayout> transformLayouts(m_swapchainImages.size(), m_depthPipeline->m_descSetLayouts[2]); // TODO 0 is the global for now, maybe make it so that each pipeline doesnt store a copy of this layout

		uint32_t previousSize = m_transformDescSets.size();
		for(uint32_t i = 0; i < m_swapchainImages.size(); ++i)
		{
			m_transformDescSets.push_back(VK_NULL_HANDLE);
		}
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool		= m_descriptorPool;
		allocInfo.descriptorSetCount	= static_cast<uint32_t>(transformLayouts.size());
		allocInfo.pSetLayouts			= transformLayouts.data();

		VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, &m_transformDescSets[previousSize]), "Failed to allocate global descriptor sets");

		for(uint32_t i=0; i < m_swapchainImages.size(); ++i)
		{
			VkDescriptorBufferInfo	bufferI = {};
			bufferI.buffer	= m_ubAllocators["transforms" + std::to_string(i)]->GetBuffer(id);
			bufferI.offset	= 0;
			bufferI.range	= m_ubAllocators["transforms" + std::to_string(i)]->GetObjSize();

			VkWriteDescriptorSet writeDS = {};
			writeDS.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDS.dstSet			= m_transformDescSets[previousSize + i];
			writeDS.dstBinding		= 0;
			writeDS.descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			writeDS.descriptorCount	= 1;
			writeDS.pBufferInfo		= &bufferI;

			vkUpdateDescriptorSets(m_device, 1, &writeDS, 0, nullptr);

		}
	}
}

void Renderer::OnMeshComponentRemoved(const ComponentRemoved<Mesh>* e)
{
	m_ecs->componentManager->RemoveComponent<Renderable>(e->entity);

	for(uint32_t i = 0; i < m_swapchainImages.size(); ++i)
	{
		uint32_t id = m_ecs->componentManager->GetComponent<Transform>(e->entity)->ub_id;
		m_ubAllocators["transforms" + std::to_string(i)]->Free(id);
	}

}

void Renderer::OnMaterialComponentAdded(const ComponentAdded<Material>* e)
{
	/*auto it = m_pipelinesRegistry.find(e->component->shaderName);
	if(it == m_pipelinesRegistry.end())
	{
		LOG_ERROR("Pipeline for {0} material doesn't exist", e->component->shaderName);
		assert(false);
	}

	m_ecs->componentManager->Sort<Material>([&](Material* lhs, Material* rhs)
			{
				return *m_pipelinesRegistry[lhs->shaderName] < *m_pipelinesRegistry[rhs->shaderName];
			});
	*/
}

void Renderer::OnDirectionalLightAdded(const ComponentAdded<DirectionalLight>* e)
{
	uint32_t slot = 0;
	for(auto& buffer : m_lightsBuffers)
		slot = buffer->Allocate(); // slot should be the same for all of these, since we allocate to every buffer every time

	e->component->_slot = slot;
	
	m_lightMap[e->component->GetComponentID()] = {0}; // 0 = DirectionalLight
	Light* light = &m_lightMap[e->component->GetComponentID()];

	for(auto& list : m_newlyAddedLights)
	{
		list.push_back({slot, light});
	}
	
	m_computePushConstants.lightNum++;
	
}

void Renderer::OnPointLightAdded(const ComponentAdded<PointLight>* e)
{
	uint32_t slot = 0;
	for(auto& buffer : m_lightsBuffers)
		slot = buffer->Allocate(); // slot should be the same for all of these, since we allocate to every buffer every time

	e->component->_slot = slot;
	
	m_lightMap[e->component->GetComponentID()] = { 1};  // 1 = PointLight
	Light* light = &m_lightMap[e->component->GetComponentID()];

	for(auto& list : m_newlyAddedLights)
	{
		list.push_back({slot, light});
	}
	m_computePushConstants.lightNum++;
}

void Renderer::OnSpotLightAdded(const ComponentAdded<SpotLight>* e)
{
	uint32_t slot = 0;
	for(auto& buffer : m_lightsBuffers)
		slot = buffer->Allocate(); // slot should be the same for all of these, since we allocate to every buffer every time

	e->component->_slot = slot;
	
	m_lightMap[e->component->GetComponentID()] = {2}; // 2 = SpotLight
	Light* light = &m_lightMap[e->component->GetComponentID()];

	for(auto& list : m_newlyAddedLights)
	{
		list.push_back({slot, light});
	}
	m_computePushConstants.lightNum++;
}


// #################################################################################

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

std::vector<const char*> GetExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#if 1
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
	return extensions;
}

VkPhysicalDevice PickDevice(const std::vector<VkPhysicalDevice>& devices, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions)
{
	std::multimap<int, VkPhysicalDevice> ratedDevices;
	for(auto& device : devices)
	{
		int score = RateDevice(device, surface, deviceExtensions);
		ratedDevices.insert(std::make_pair(score, device));
	}

	if(ratedDevices.rbegin()->first > 0)
		return ratedDevices.rbegin()->second;
	else
		throw std::runtime_error("Didn't find suitable GPU");
}

int RateDevice(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions)
{
	uint32_t extensionPropCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionPropCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionPropCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionPropCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	bool swapChainGood = false;
	if(requiredExtensions.empty())
	{

		SwapchainSupportDetails details = QuerySwapChainSupport(device, surface);
		swapChainGood = !details.formats.empty() && !details.presentModes.empty();

	}

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);


	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	int score = 0;
	if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		score += 1000;

	score += deviceProperties.limits.maxImageDimension2D;
	bool temp = FindQueueFamilies(device, surface).IsComplete();
	// example: if the application cannot function without a geometry shader
	if( !deviceFeatures.geometryShader ||
			!temp ||
			!requiredExtensions.empty() ||
			!swapChainGood ||
			!deviceFeatures.samplerAnisotropy)
	{
		return 0;
	}
	LOG_TRACE("Device: {0}    Score: {1}", deviceProperties.deviceName, score);
	return score;
}

VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);

	VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT)  { return VK_SAMPLE_COUNT_8_BIT;  }
	if (counts & VK_SAMPLE_COUNT_4_BIT)  { return VK_SAMPLE_COUNT_4_BIT;  }
	if (counts & VK_SAMPLE_COUNT_2_BIT)  { return VK_SAMPLE_COUNT_2_BIT;  }

	return VK_SAMPLE_COUNT_1_BIT;
}

QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	QueueFamilyIndices indices;
	uint32_t count;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queueFamilies.data());
	int i = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		if(queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}
		VkBool32 presentationSupport;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
		if(queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;
		}
		if(indices.IsComplete())
		{
			break;
		}

		i++;
	}
	return indices;
}

SwapchainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapchainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
	details.formats.resize(count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, details.formats.data());

	count = 0; // this is probably not needed but i will put it in just in case for now
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
	details.presentModes.resize(count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, details.presentModes.data());

	return details;
}

VkSurfaceFormatKHR ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{

	// if the surface has no preferred format vulkan returns one entity of Vk_FORMAT_UNDEFINED
	// we can then chose whatever we want
	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		VkSurfaceFormatKHR format;
		format.colorSpace   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		format.format       = VK_FORMAT_B8G8R8A8_UNORM;
		return format;
	}
	for(const auto& format : availableFormats)
	{
		if(format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return format;
	}

	// if we can't find one that we like we could rank them based on how good they are
	// but we will just settle for the first one(apparently in most cases it's okay)
	return availableFormats[0];
}

VkPresentModeKHR ChooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	for(const auto& presentMode : availablePresentModes)
	{
		if(presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			return presentMode;
	}

	// FIFO is guaranteed to be available
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
{
	// swap extent is the resolution of the swapchain images

	// if we can set an extent manually the width and height values will be uint32t max
	// else we can't set it so just return it
	if(capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) // () around max to prevent macro expansion by windows.h max macro
		return capabilities.currentExtent;
	else
	{

		// choose an extent within the minImageExtent and maxImageExtent bounds
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
		actualExtent.width = glm::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = glm::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;

	}
}

VkBool32 debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData, void * /*pUserData*/)
{
	
	std::ostringstream message;
	message << "\n";
	message << "\t" << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
	message << "\t" << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
	message << "\t" << "message         = <" << pCallbackData->pMessage << ">\n";

	if (0 < pCallbackData->queueLabelCount)
	{
		message << "\t" << "Queue Labels:\n";
		for (uint8_t i = 0; i < pCallbackData->queueLabelCount; i++)
		{
			message << "\t\t" << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
		}
	}
	if (0 < pCallbackData->cmdBufLabelCount)
	{
		message << "\t" << "CommandBuffer Labels:\n";
		for (uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++)
		{
			message << "\t\t" << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
		}
	}
	if (0 < pCallbackData->objectCount)
	{
		message << "\t" << "Objects:\n";
		for (uint8_t i = 0; i < pCallbackData->objectCount; i++)
		{
			message << "\t\t" << "Object " << i << "\n";
			message << "\t\t\t" << "objectType   = " << pCallbackData->pObjects[i].objectType << "\n";
			message << "\t\t\t" << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
			if (pCallbackData->pObjects[i].pObjectName)
			{
				message << "\t\t\t" << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
			}
		}
	}
	
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
		LOG_TRACE(message.str());
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT && strstr(pCallbackData->pMessageIdName, "DEBUG-PRINTF") != NULL) {
		LOG_INFO(message.str());
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		LOG_WARN(message.str());
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		LOG_ERROR(message.str());
	}

	return VK_TRUE;
}
