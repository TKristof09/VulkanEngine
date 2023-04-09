#include "Rendering/Renderer.hpp"
#include "Rendering/Image.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <memory>
#include <sstream>
#include <optional>
#include <set>
#include <map>
#include <limits>
#include <chrono>
#include <array>
#include <string>
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "TextureManager.hpp"
#include "ECS/ComponentManager.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "ECS/CoreComponents/Lights.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "ECS/CoreComponents/Renderable.hpp"
#include "ECS/CoreComponents/Material.hpp"

const uint32_t MAX_FRAMES_IN_FLIGHT = 3;


struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentationFamily;
	std::optional<uint32_t> computeFamily;

	bool IsComplete() {
		return graphicsFamily.has_value() && presentationFamily.has_value() && computeFamily.has_value();
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
	m_computePushConstants.tileNums = glm::ceil(glm::vec2(m_computePushConstants.viewportSize) / 16.f); // TODO Make this take into account TILE_SIZE from common.glsl
	m_computePushConstants.lightNum = 0;
	m_changedLights.resize(m_swapchainImages.size());

	m_transformDescSets.resize(m_swapchainImages.size());

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
    UpdateShadowDescriptors();
	CreateDebugUI();
	CreateCommandBuffers();
	CreateSyncObjects();


    m_rendererDebugWindow = std::make_unique<DebugUIWindow>("Renderer");
    AddDebugUIWindow(m_rendererDebugWindow.get());
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

	for(auto& pool : m_queryPools)
	{
		vkDestroyQueryPool(m_device, pool, nullptr);
	}
	TextureManager::ClearLoadedTextures();

	CleanupSwapchain();

	vkDestroySampler(m_device, m_computeSampler, nullptr);

	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	vkDestroyCommandPool(m_device, m_commandPool, nullptr);

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);



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
    UpdateShadowDescriptors();



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


    //VkValidationFeatureEnableEXT enables[] = { VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT}
    VkValidationFeatureEnableEXT enables[] = { VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT, VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT };
    VkValidationFeaturesEXT features = {};
    features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    features.enabledValidationFeatureCount = 2;
    features.pEnabledValidationFeatures = enables;

    debugCreateInfo.pNext = &features;
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

	CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &VulkanContext::m_messenger);
}

void Renderer::CreateDevice()
{
	std::vector<const char*> deviceExtensions;
	deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifdef VDEBUG
	deviceExtensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
#endif

	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	m_gpu = PickDevice(devices, m_surface, deviceExtensions);

	vkGetPhysicalDeviceProperties(m_gpu, &VulkanContext::m_gpuProperties);
	LOG_TRACE("Picked device: {0}", VulkanContext::m_gpuProperties.deviceName);

	m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;//GetMaxUsableSampleCount(m_gpu); MSAA causes flickering bug

	QueueFamilyIndices families = FindQueueFamilies(m_gpu, m_surface);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies          = {families.graphicsFamily.value(), families.presentationFamily.value(), families.computeFamily.value()};

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

    VkPhysicalDeviceVulkan12Features device12Features = {};
    device12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    device12Features.imagelessFramebuffer = VK_TRUE;
    // these are for dynamic descriptor indexing
    deviceFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
	device12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	device12Features.runtimeDescriptorArray = VK_TRUE;
	device12Features.descriptorBindingPartiallyBound = VK_TRUE;
	device12Features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;






	VkDeviceCreateInfo createInfo		= {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.enabledExtensionCount    = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames  = deviceExtensions.data();
	createInfo.pEnabledFeatures         = &deviceFeatures;
	createInfo.pQueueCreateInfos        = queueCreateInfos.data();
	createInfo.queueCreateInfoCount     = static_cast<uint32_t>(queueCreateInfos.size());

	createInfo.pNext = &device12Features;

	VK_CHECK(vkCreateDevice(m_gpu, &createInfo, nullptr, &m_device), "Failed to create device");
	vkGetDeviceQueue(m_device, families.graphicsFamily.value(), 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_device, families.presentationFamily.value(), 0, &m_presentQueue);
	vkGetDeviceQueue(m_device, families.computeFamily.value(), 0, &m_computeQueue);


	m_queryPools.resize(MAX_FRAMES_IN_FLIGHT);
	m_queryResults.resize(MAX_FRAMES_IN_FLIGHT);
	for(uint32_t i=0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{

		VkQueryPoolCreateInfo queryCI = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		queryCI.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryCI.queryCount = 4;
		vkCreateQueryPool(m_device, &queryCI, nullptr, &m_queryPools[i]);

	}
	m_timestampPeriod = VulkanContext::m_gpuProperties.limits.timestampPeriod;
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
	colorAttachment.clearValue.color = {0.f, 0.f, 0.f, 1.0f};


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
	colorAttachmentResolve.type = RenderPassAttachmentType::COLOR_RESOLVE;
	colorAttachmentResolve.format = m_swapchainImageFormat;
	colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	colorAttachmentResolve.internalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDependency2 dependency = {};
	dependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	RenderPassCreateInfo ci;
	ci.attachments = {colorAttachment, depthAttachment, colorAttachmentResolve };
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
	prePassDepthAttachment.internalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    prePassDepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	prePassDepthAttachment.clearValue.depthStencil = {0.0f, 0 };

	RenderPassAttachment prePassDepthResolve = {};
	prePassDepthResolve.type = RenderPassAttachmentType::DEPTH_RESOLVE;
	prePassDepthResolve.format = VK_FORMAT_D32_SFLOAT;
	prePassDepthResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	prePassDepthResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	prePassDepthResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	prePassDepthResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	prePassDepthResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	prePassDepthResolve.internalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    prePassDepthResolve.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;


	std::vector<VkSubpassDependency2> dependencies(2);

    // TODO might not need this dependency, since the prepass doesn't use any attachments from previous renderpasses, but maybe we wait for other renderpasses to finish reading from the depth image before writing to it again?
	dependencies[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // resolve write and depth write (including layout transitions )
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	RenderPassCreateInfo prepassCi;
	prepassCi.attachments = {prePassDepthAttachment, prePassDepthResolve};
	prepassCi.msaaSamples = m_msaaSamples;
	prepassCi.subpassCount = 1;
	prepassCi.dependencies = dependencies;

	m_prePassRenderPass = RenderPass(prepassCi);
	LOG_TRACE("Created prepass render pass");


	// Render pass for the depth prepass
	RenderPassAttachment shadowPassDepthAttachment = {};
	shadowPassDepthAttachment.type = RenderPassAttachmentType::DEPTH;
	shadowPassDepthAttachment.format = VK_FORMAT_D32_SFLOAT;
	shadowPassDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	shadowPassDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	shadowPassDepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	shadowPassDepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	shadowPassDepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	shadowPassDepthAttachment.internalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    shadowPassDepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	shadowPassDepthAttachment.clearValue.depthStencil = {0.0f, 0 };


	std::vector<VkSubpassDependency2> shadowDependencies(2);

    // TODO might not need this dependency, since the prepass doesn't use any attachments from previous renderpasses, but maybe we wait for other renderpasses to finish reading from the depth image before writing to it again?
	shadowDependencies[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
	shadowDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	shadowDependencies[0].dstSubpass = 0;
	shadowDependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	shadowDependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	shadowDependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	shadowDependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	shadowDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	shadowDependencies[1].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
	shadowDependencies[1].srcSubpass = 0;
	shadowDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	shadowDependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    shadowDependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // depth write (including layout transitions )
	shadowDependencies[1].dstStageMask =  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	shadowDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	shadowDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	RenderPassCreateInfo shadowPassCi;
	shadowPassCi.attachments = {shadowPassDepthAttachment};
	shadowPassCi.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	shadowPassCi.subpassCount = 1;
	shadowPassCi.dependencies = shadowDependencies;

	m_shadowRenderPass = RenderPass(shadowPassCi);
	LOG_TRACE("Created shadow pass render pass");
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
	depthPipeline.depthCompareOp	= VK_COMPARE_OP_GREATER;
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

    // Shadow prepass pipeline
	LOG_WARN("Creating shadow pipeline");
	PipelineCreateInfo shadowPipeline;
	shadowPipeline.type				= PipelineType::GRAPHICS;
	//depthPipeline.parent			= const_cast<Pipeline*>(&(*mainIter));
	shadowPipeline.depthCompareOp	= VK_COMPARE_OP_GREATER;
	shadowPipeline.depthWriteEnable	= true;
	shadowPipeline.useDepth			= true;
	shadowPipeline.depthClampEnable = false;
	shadowPipeline.msaaSamples		= VK_SAMPLE_COUNT_1_BIT;
	shadowPipeline.renderPass		= &m_shadowRenderPass;
	shadowPipeline.subpass			= 0;
	shadowPipeline.stages			= VK_SHADER_STAGE_VERTEX_BIT;
	shadowPipeline.useMultiSampling	= true;
	shadowPipeline.useColorBlend	= false;
	shadowPipeline.viewportExtent	= {1024,1024};

	shadowPipeline.isGlobal			= true;

	m_shadowPipeline = std::make_unique<Pipeline>("shadow", shadowPipeline); // depth shader is fine for the shadow maps too since we only need the depth info from it

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
		ci.attachmentDescriptions = {};
		ci.attachmentDescriptions.push_back({ m_depthImage, {}});
		if(m_msaaSamples != VK_SAMPLE_COUNT_1_BIT)
            ci.attachmentDescriptions.push_back({ m_resolvedDepthImage, {}});
		m_prePassRenderPass.AddFramebuffer(ci);


		ci = {};
		ci.width = SHADOWMAP_SIZE;
		ci.height = SHADOWMAP_SIZE;
		ci.attachmentDescriptions = {};
        ci.imageless = true;

        ImageCreateInfo imageCi;
        imageCi.format = VK_FORMAT_D32_SFLOAT;
        imageCi.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
        imageCi.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        imageCi.useMips = false;
        imageCi.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		ci.attachmentDescriptions.push_back({{}, imageCi});
		m_shadowRenderPass.AddFramebuffer(ci);
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


    VkDescriptorSetAllocateInfo sallocInfo = {};
	std::vector<VkDescriptorSetLayout> slayouts;

	if(m_shadowDesc.size() != 0)
		return;
	for (int i = 0; i < m_swapchainImages.size(); ++i)
	{
        slayouts.push_back(m_shadowPipeline->m_descSetLayouts[0]);
	}
	sallocInfo.sType				= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	sallocInfo.descriptorPool		= m_descriptorPool;
	sallocInfo.descriptorSetCount	= static_cast<uint32_t>(slayouts.size());
	sallocInfo.pSetLayouts			= slayouts.data();
	m_shadowDesc.resize(slayouts.size());
	VK_CHECK(vkAllocateDescriptorSets(VulkanContext::GetDevice(), &sallocInfo, m_shadowDesc.data()), "Failed to allocate descriptor sets");

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
	ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
	ci.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	ci.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	ci.msaaSamples = m_msaaSamples;
	ci.useMips = false;
    if(m_msaaSamples == VK_SAMPLE_COUNT_1_BIT)
        ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ;
	m_depthImage = std::make_shared<Image>(m_swapchainExtent, ci);

	ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ;
	ci.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	m_resolvedDepthImage = std::make_shared<Image>(m_swapchainExtent, ci);


	ci = {};
	ci.format = VK_FORMAT_B8G8R8A8_UNORM;
	ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ci.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	ci.layout = VK_IMAGE_LAYOUT_GENERAL;
	ci.useMips = false;
	m_lightCullDebugImage = std::make_shared<Image>(m_swapchainExtent, ci);

}

void Renderer::UpdateComputeDescriptors()
{

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
		depthImageInfo.imageLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; // TODO: depth_read_only_optimal
        if(m_msaaSamples != VK_SAMPLE_COUNT_1_BIT)
		    depthImageInfo.imageView	= m_resolvedDepthImage->GetImageView();
        else
		    depthImageInfo.imageView	= m_depthImage->GetImageView();
		depthImageInfo.sampler		= m_computeSampler;

		VkDescriptorImageInfo debugImageInfo = {};
		debugImageInfo.imageLayout	= VK_IMAGE_LAYOUT_GENERAL;
		debugImageInfo.imageView	= m_lightCullDebugImage->GetImageView();
		debugImageInfo.sampler		= VK_NULL_HANDLE;

		std::array<VkWriteDescriptorSet, 4> writeDS({});
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

		writeDS[3].sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDS[3].dstSet			= m_computeDesc[i];
		writeDS[3].dstBinding		= 4;
		writeDS[3].descriptorType	= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writeDS[3].descriptorCount	= 1;
		writeDS[3].pImageInfo		= &debugImageInfo;

		vkUpdateDescriptorSets(m_device, writeDS.size(), writeDS.data(), 0, nullptr);
	}

}
void Renderer::UpdateShadowDescriptors()
{

	for(uint32_t i=0; i < m_swapchainImages.size(); ++i)
	{

		VkDescriptorBufferInfo lightsBI = {};
		lightsBI.offset	= 0;
		lightsBI.range	= VK_WHOLE_SIZE;
		lightsBI.buffer	= m_lightsBuffers[i]->GetBuffer(0);


		VkWriteDescriptorSet writeDS = {};

		writeDS.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDS.dstSet			= m_shadowDesc[i];
		writeDS.dstBinding		= 1;
		writeDS.descriptorType	= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDS.descriptorCount	= 1;
		writeDS.pBufferInfo		= &lightsBI;

        vkUpdateDescriptorSets(m_device, 1, &writeDS, 0, nullptr);
	}

}

void Renderer::UpdateLights(uint32_t index)
{

	PROFILE_FUNCTION();
	{ PROFILE_SCOPE("Loop #1");

    glm::mat4 proj = glm::ortho(-500.f, 500.f, -500.f, 500.f, 500.f, -500.f);
	for(auto* lightComp : m_ecs->componentManager->GetComponents<DirectionalLight>())
	{
		Transform* t = m_ecs->componentManager->GetComponent<Transform>(lightComp->GetOwner());
		glm::mat4 tMat = t->GetTransform();

		glm::vec3 newDir = glm::normalize(-glm::vec3(tMat[2]));
		float newIntensity = lightComp->intensity;
		glm::vec3 newColor = lightComp->color.ToVec3();


		Light& light = m_lightMap[lightComp->GetComponentID()];
		bool changed = newDir != light.direction || newIntensity != light.intensity || newColor != light.color;
		if(changed)
		{

			light.direction = newDir;
			light.intensity = newIntensity;
			light.color = newColor;
            glm::mat4 viewMat = glm::inverse(tMat); //might be better to construct it with -pos and -rot instead of using inverse
            glm::mat4 vpMat = proj * viewMat;
            light.lightSpaceMatrix = vpMat;

			for(auto& dict : m_changedLights)
				dict[lightComp->_slot] =  &light; // if it isn't in the dict then put it in otherwise just replace it (which actually does nothing but we would have to check if the dict contains it anyway so shouldn't matter for perf)
		}
	}
	}
	{ 		PROFILE_SCOPE("Loop #2");

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

			for(auto& dict : m_changedLights)
				dict[lightComp->_slot] =  &light; // if it isn't in the dict then put it in otherwise just replace it (which actually does nothing but we would have to check if the dict contains it anyway so shouldn't matter for perf)
		}
	}
	}
	{		PROFILE_SCOPE("Loop #3");

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

			for(auto& dict : m_changedLights)
				dict[lightComp->_slot] =  &light; // if it isn't in the dict then put it in otherwise just replace it (which actually does nothing but we would have to check if the dict contains it anyway so shouldn't matter for perf)
		}
	}
	}

	std::vector<std::pair<uint32_t, void*>> slotsAndDatas;
	for(auto& [slot, newLight] : m_changedLights[index])
	{
		slotsAndDatas.push_back({slot, newLight});
	}
	m_changedLights[index].clear();

	m_lightsBuffers[index]->UpdateBuffer(slotsAndDatas);

}


void Renderer::Render(double dt)
{


	PROFILE_FUNCTION();
	uint32_t imageIndex;
	VkResult result;
	{
		PROFILE_SCOPE("Pre frame stuff");
		vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

		result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &imageIndex);


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
	}
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
	{
		PROFILE_SCOPE("Command Recording and submitting");
		vkCmdResetQueryPool(cb, m_queryPools[m_currentFrame], 0, 4);

		vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_queryPools[m_currentFrame],0);




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
        {
			PROFILE_SCOPE("Prepass draw call loop");
			for(auto* renderable : cm->GetComponents<Renderable>())
			{
				auto transform  = cm->GetComponent<Transform>(renderable->GetOwner());
				glm::mat4 model =  transform->GetTransform();
				{
					PROFILE_SCOPE("Bind buffers");
					renderable->vertexBuffer.Bind(m_mainCommandBuffers[imageIndex]);
					renderable->indexBuffer.Bind(m_mainCommandBuffers[imageIndex]);
				}
				uint32_t offset;
				uint32_t bufferIndex = m_ubAllocators["transforms" + std::to_string(imageIndex)]->GetBufferIndexAndOffset(transform->ub_id, offset);

				m_ubAllocators["transforms" + std::to_string(imageIndex)]->UpdateBuffer(transform->ub_id, &model);

				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthPipeline->m_layout, 2, 1, &m_transformDescSets[imageIndex][bufferIndex], 1, &offset);
				vkCmdDrawIndexed(cb, static_cast<uint32_t>(renderable->indexBuffer.GetSize() / sizeof(uint32_t)) , 1, 0, 0, 0); // TODO: make the size calculation better (not hardcoded for uint32_t)
			}
		}
		vkCmdEndRenderPass(cb);
		vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPools[m_currentFrame], 1);

        auto shadowBegin = m_shadowRenderPass.GetBeginInfo(0);
        {
			PROFILE_SCOPE("Shadow pass draw call loop");
            for(auto* light : cm->GetComponents<DirectionalLight>())
            {

                VkRenderPassAttachmentBeginInfo attachmentBegin = {};
                attachmentBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO;
                attachmentBegin.attachmentCount = 1;
                VkImageView view = m_shadowmaps[light->_shadowSlot]->GetImageView();
                attachmentBegin.pAttachments = &view;
                shadowBegin.pNext = &attachmentBegin;

                vkCmdBeginRenderPass(cb, &shadowBegin, VK_SUBPASS_CONTENTS_INLINE);
        		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline->m_pipeline);
                uint32_t offset = 0;
		        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline->m_layout, 0, 1, &m_shadowDesc[imageIndex], 1, &offset);

                vkCmdPushConstants(cb, m_shadowPipeline->m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &light->_slot);
                for(auto* renderable : cm->GetComponents<Renderable>())
                {
                    auto transform  = cm->GetComponent<Transform>(renderable->GetOwner());
                    {
                        PROFILE_SCOPE("Bind buffers");
                        renderable->vertexBuffer.Bind(m_mainCommandBuffers[imageIndex]);
                        renderable->indexBuffer.Bind(m_mainCommandBuffers[imageIndex]);
                    }
                    uint32_t offset;
                    uint32_t bufferIndex = m_ubAllocators["transforms" + std::to_string(imageIndex)]->GetBufferIndexAndOffset(transform->ub_id, offset);


                    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline->m_layout, 2, 1, &m_transformDescSets[imageIndex][bufferIndex], 1, &offset);
                    vkCmdDrawIndexed(cb, static_cast<uint32_t>(renderable->indexBuffer.GetSize() / sizeof(uint32_t)) , 1, 0, 0, 0); // TODO: make the size calculation better (not hardcoded for uint32_t)
                }
                vkCmdEndRenderPass(cb);
            }
		}
        //TODO timestamp



        std::vector<VkBufferMemoryBarrier> barriersBefore(2);
        barriersBefore[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriersBefore[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriersBefore[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriersBefore[0].buffer = m_lightsBuffers[imageIndex]->GetBuffer(0);
        barriersBefore[0].size = m_lightsBuffers[imageIndex]->GetSize();
        barriersBefore[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriersBefore[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriersBefore[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriersBefore[1].buffer = m_visibleLightsBuffers[imageIndex]->GetBuffer(0);
        barriersBefore[1].size = m_visibleLightsBuffers[imageIndex]->GetSize();

        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, barriersBefore.size(), barriersBefore.data(), 0, nullptr);
		uint32_t offset = 0;
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute->m_pipeline);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute->m_layout, 0, 1, &m_computeDesc[imageIndex], 1, &offset);


		m_computePushConstants.debugMode = 1;
		vkCmdPushConstants(cb, m_compute->m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &m_computePushConstants);
		vkCmdDispatch(cb, m_computePushConstants.tileNums.x, m_computePushConstants.tileNums.y, 1);

        std::vector<VkBufferMemoryBarrier> barriersAfter(2);
        barriersAfter[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriersAfter[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriersAfter[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriersAfter[0].buffer = m_lightsBuffers[imageIndex]->GetBuffer(0);
        barriersAfter[0].size = m_lightsBuffers[imageIndex]->GetSize();
        barriersAfter[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriersAfter[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriersAfter[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriersAfter[1].buffer = m_visibleLightsBuffers[imageIndex]->GetBuffer(0);
        barriersAfter[1].size = m_visibleLightsBuffers[imageIndex]->GetSize();
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, barriersAfter.size(), barriersAfter.data(), 0, nullptr);

		vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_queryPools[m_currentFrame], 2);

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


			PROFILE_SCOPE("Draw call loop");
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
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_layout, 2, 1, &m_transformDescSets[imageIndex][bufferIndex], 1, &offset);


				if(!pipeline.m_isGlobal)
				{
					m_ubAllocators[pipeline.m_name + "material" + std::to_string(imageIndex)]->GetBufferIndexAndOffset(materialIterator->_ubSlot, offset);

					glm::vec4 ubs[] = {{materialIterator->_textureSlot, 0, 0, 0}, {0.5f, 0,0,1.0f}, };

					m_ubAllocators[pipeline.m_name + "material" + std::to_string(imageIndex)]->UpdateBuffer(materialIterator->_ubSlot, ubs);

				}
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_layout, 1, 1, &m_descriptorSets[pipeline.m_name + std::to_string(1)][bufferIndex + imageIndex], 1, &offset);
				vkCmdDrawIndexed(cb, static_cast<uint32_t>(renderable->indexBuffer.GetSize() / sizeof(uint32_t)) , 1, 0, 0, 0); // TODO: make the size calculation better (not hardcoded for uint32_t)


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
		vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPools[m_currentFrame], 3);
		m_mainCommandBuffers[imageIndex].End();

		VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
//
//	VkSubmitInfo submitInfo = {};
//	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//	submitInfo.commandBufferCount = 1;
//	submitInfo.pCommandBuffers = &cb;
//	submitInfo.waitSemaphoreCount = 1;
//	submitInfo.pWaitSemaphores = &m_imageAvailable[m_currentFrame];
//	submitInfo.pWaitDstStageMask = &wait;
//	submitInfo.signalSemaphoreCount = 1;
//	submitInfo.pSignalSemaphores = &m_renderFinished[m_currentFrame];
//
//	VK_CHECK(vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]), "Failed to reset fence");
//	VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]), "Failed to submit command buffers");
		m_mainCommandBuffers[imageIndex].Submit(m_graphicsQueue, m_imageAvailable[m_currentFrame], wait, m_renderFinished[m_currentFrame], m_inFlightFences[m_currentFrame]);

	}

	{
		PROFILE_SCOPE("Present");
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_renderFinished[m_currentFrame];

		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_swapchain;
		presentInfo.pImageIndices = &imageIndex;
		result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
	}
	std::array<uint64_t, 4> queryResults;
	{
		PROFILE_SCOPE("Query results");
		//VK_CHECK(vkGetQueryPoolResults(m_device, m_queryPools[m_currentFrame], 0, 4, sizeof(uint64_t) * 4, queryResults.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "Query isn't ready");

	}
    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window->IsResized())
    {
        m_window->SetResized(false);
        RecreateSwapchain();
    }
    else if(result != VK_SUCCESS)
        throw std::runtime_error("Failed to present swap chain image");

    m_queryResults[m_currentFrame] = (queryResults[3] - queryResults[0]) * m_timestampPeriod;
    //LOG_INFO("GPU took {0} ms", m_queryResults[m_currentFrame] * 1e-6);

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

	uint32_t slot;
	for(uint32_t i = 0; i < m_swapchainImages.size(); ++i)
	{
		slot = m_ubAllocators["transforms" + std::to_string(i)]->Allocate();
		m_ecs->componentManager->GetComponent<Transform>(e->entity)->ub_id = slot;



		uint32_t offset;
		uint32_t bufferIndex = m_ubAllocators["transforms" + std::to_string(i)]->GetBufferIndexAndOffset(slot, offset);
		if(bufferIndex >= m_transformDescSets[i].size())
		{
			VkDescriptorSetLayout transformLayouts = { m_depthPipeline->m_descSetLayouts[2]}; // TODO 0 is the global for now, maybe make it so that each pipeline doesnt store a copy of this layout

			m_transformDescSets[i].push_back(VK_NULL_HANDLE);

			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool		= m_descriptorPool;
			allocInfo.descriptorSetCount	= 1;
			allocInfo.pSetLayouts			= &transformLayouts;

			VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, &m_transformDescSets[i][bufferIndex]), "Failed to allocate global descriptor sets");


			VkDescriptorBufferInfo	bufferI = {};
			bufferI.buffer	= m_ubAllocators["transforms" + std::to_string(i)]->GetBuffer(slot);
			bufferI.offset	= 0;
			bufferI.range	= sizeof(glm::mat4);

			VkWriteDescriptorSet writeDS = {};
			writeDS.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDS.dstSet			= m_transformDescSets[i][bufferIndex];
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

	for(auto& dict : m_changedLights)
	{
		dict[slot] = light;
	}

	m_computePushConstants.lightNum++;

    ImageCreateInfo ci;
    ci.format = VK_FORMAT_D32_SFLOAT;
    ci.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    ci.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; // the render pass will transfer to good layout
    ci.useMips = false;
    ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    slot = m_shadowmaps.size();
    e->component->_shadowSlot = slot;
    m_shadowmaps.push_back(std::make_unique<Image>(SHADOWMAP_SIZE, SHADOWMAP_SIZE, ci));

    light->shadowSlot = slot;
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; // TODO: depth_read_only_optimal
    imageInfo.imageView	= m_shadowmaps[slot]->GetImageView();
    imageInfo.sampler	= m_computeSampler;

    std::vector<VkWriteDescriptorSet> writeDSVector;
    for (int i = 0; i < m_swapchainImages.size(); ++i) {

        VkWriteDescriptorSet writeDS = {};
        writeDS.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDS.dstSet			= m_tempDesc[i];
        writeDS.dstBinding		= 3;
        writeDS.descriptorType	= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDS.descriptorCount	= 1;
        writeDS.pImageInfo		= &imageInfo;

        writeDSVector.push_back(writeDS);

    }
    vkUpdateDescriptorSets(m_device, writeDSVector.size(), writeDSVector.data(), 0, nullptr);

    m_rendererDebugWindow->AddElement(std::make_shared<UIImage>(m_shadowmaps[slot].get()));

}

void Renderer::OnPointLightAdded(const ComponentAdded<PointLight>* e)
{
	uint32_t slot = 0;
	for(auto& buffer : m_lightsBuffers)
		slot = buffer->Allocate(); // slot should be the same for all of these, since we allocate to every buffer every time

	e->component->_slot = slot;

	m_lightMap[e->component->GetComponentID()] = { 1};  // 1 = PointLight
	Light* light = &m_lightMap[e->component->GetComponentID()];

	for(auto& dict : m_changedLights)
	{
		dict[slot] = light;
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

	for(auto& list : m_changedLights)
	for(auto& dict : m_changedLights)
	{
		dict[slot] = light;
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



std::vector<const char*> GetExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#ifdef VDEBUG
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
		if(queueFamily.queueCount > 0 && presentationSupport && !indices.presentationFamily.has_value())
		{
			indices.presentationFamily = i;
		}

		if(queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
			indices.computeFamily = i;

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
		if(presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
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

	std::string messageidName = "";
	if(pCallbackData->pMessageIdName)
		messageidName = pCallbackData->pMessageIdName;

	message << "\t" << "messageIDName   = <" << messageidName << ">\n";


	message << "\t" << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";

	if(pCallbackData->pMessage)
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
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT && messageidName.find("DEBUG-PRINTF") != std::string::npos) {
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
