#include "ECS/CoreSystems/RendererSystem.hpp"
#include <sstream>
#include <optional>
#include <set>
#include <map>
#include <limits>
#include <chrono>
#include <array>
#include <vulkan/vulkan_core.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "ECS/ComponentManager.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "ECS/CoreComponents/Renderable.hpp"
#include "ECS/CoreComponents/Material.hpp"


const uint32_t MAX_FRAMES_IN_FLIGHT = 2;
const uint32_t OBJECTS_PER_DESCRIPTOR_CHUNK = 500;

VkPhysicalDevice VulkanContext::m_gpu = VK_NULL_HANDLE;
VkDevice VulkanContext::m_device = VK_NULL_HANDLE;


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




RendererSystem::RendererSystem(std::shared_ptr<Window> window):
	m_window(window),
	m_gpu(VulkanContext::m_gpu),
	m_device(VulkanContext::m_device)
{
	Subscribe(&RendererSystem::OnMeshComponentAdded);
	Subscribe(&RendererSystem::OnMeshComponentRemoved);
	Subscribe(&RendererSystem::OnMaterialComponentAdded);

	CreateInstance();
	SetupDebugMessenger();
	CreateDevice();
	m_ubAllocators["baseUniformBufferObject"] = std::move(std::make_unique<UniformBufferAllocator>(m_gpu, m_device, sizeof(glm::mat4), OBJECTS_PER_DESCRIPTOR_CHUNK));
	m_ubAllocators["depthUniformBufferObject"] = std::move(std::make_unique<UniformBufferAllocator>(m_gpu, m_device, sizeof(glm::mat4), OBJECTS_PER_DESCRIPTOR_CHUNK));
	CreateSwapchain();
	CreateRenderPass();
	CreateDescriptorSetLayout();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreatePipeline();
	CreateCommandPool();
	CreateColorResources();
	CreateDepthResources();
	CreateFramebuffers();
	CreateTexture();
	CreateSampler();
	CreateUniformBuffers();
	CreateDebugUI();
	CreateCommandBuffers();
	CreateSyncObjects();

}

RendererSystem::~RendererSystem()
{
	vkDeviceWaitIdle(m_device);
	for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(m_device, m_imageAvailable[i], nullptr);
		vkDestroySemaphore(m_device, m_renderFinished[i], nullptr);
		vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
	}


	//vkDestroySampler(m_device, m_sampler, nullptr);

	CleanupSwapchain();

	vkDestroySampler(m_device, m_sampler, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

	vkDestroyCommandPool(m_device, m_commandPool, nullptr);
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

	vkDestroyDevice(m_device, nullptr);

#ifdef VDEBUG
	DestroyDebugUtilsMessengerEXT(m_instance, m_messenger, nullptr);
#endif

	vkDestroyInstance(m_instance, nullptr);



}

void RendererSystem::RecreateSwapchain()
{
	while(m_window->GetWidth() == 0 || m_window->GetHeight() == 0)
		glfwWaitEvents();


	vkDeviceWaitIdle(m_device);

	CleanupSwapchain();

	CreateSwapchain();
	CreateRenderPass();
	CreateDescriptorSetLayout();
	CreatePipeline();
	CreateColorResources();
	CreateDepthResources();
	CreateFramebuffers();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateCommandBuffers();


	DebugUIInitInfo initInfo = {};
	initInfo.pWindow = m_window;
	initInfo.instance = m_instance;
	initInfo.gpu = m_gpu;
	initInfo.device = m_device;

	QueueFamilyIndices families = FindQueueFamilies(m_gpu, m_surface);
	initInfo.queueFamily = families.graphicsFamily.value();
	initInfo.queue = m_graphicsQueue;

	initInfo.renderPass = m_renderPass;
	initInfo.pipelineCache = nullptr;
	initInfo.descriptorPool = m_descriptorPool;
	initInfo.imageCount = static_cast<uint32_t>(m_swapchainImages.size());
	initInfo.msaaSamples = m_msaaSamples;
	initInfo.allocator = nullptr;
	initInfo.commandPool = m_commandPool;
	m_debugUI->ReInit(&initInfo);

}

void RendererSystem::CleanupSwapchain()
{
	m_depthImage.reset();
	m_colorImage.reset();

	for(auto framebuffer : m_swapchainFramebuffers)
	{
		vkDestroyFramebuffer(m_device, framebuffer, nullptr);
	}
	m_commandBuffers.clear();
	m_mainCommandBuffers.clear();
	m_uniformBuffers.clear();

	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
	vkDestroyRenderPass(m_device, m_renderPass, nullptr);

	for(auto imageView : m_swapchainImageViews)
	{
		vkDestroyImageView(m_device, imageView, nullptr);
	}
	vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}

void RendererSystem::CreateDebugUI()
{

	DebugUIInitInfo initInfo = {};
	initInfo.pWindow = m_window;
	initInfo.instance = m_instance;
	initInfo.gpu = m_gpu;
	initInfo.device = m_device;

	QueueFamilyIndices families = FindQueueFamilies(m_gpu, m_surface);
	initInfo.queueFamily = families.graphicsFamily.value();
	initInfo.queue = m_graphicsQueue;

	initInfo.renderPass = m_renderPass;
	initInfo.pipelineCache = nullptr;
	initInfo.descriptorPool = m_descriptorPool;
	initInfo.imageCount = static_cast<uint32_t>(m_swapchainImages.size());
	initInfo.msaaSamples = m_msaaSamples;
	initInfo.allocator = nullptr;
	initInfo.commandPool = m_commandPool;


	m_debugUI = std::make_shared<DebugUI>(&initInfo);
}

void RendererSystem::CreateInstance()
{
	VkApplicationInfo appinfo = {};
	appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appinfo.pApplicationName = "VulkanEngine";
	appinfo.apiVersion = VK_MAKE_VERSION(1, 1, 101);
	;

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
	for (VkLayerProperties layer : instanceLayerProperties) {
		std::cout << layer.layerName << std::endl;
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
	debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugCreateInfo.pfnUserCallback = debugUtilsMessengerCallback;
	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
#endif

	VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance), "Failed to create instance");

	VK_CHECK(glfwCreateWindowSurface(m_instance, m_window->GetWindow(), nullptr, &m_surface), "Failed ot create window surface!");

}

void RendererSystem::SetupDebugMessenger()
{
#ifndef VDEBUG
	return;
#endif
	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity =  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugUtilsMessengerCallback;

	CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_messenger);
}

void RendererSystem::CreateDevice()
{
	/* uint32_t deviceCount = 0;
	   vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	   if(deviceCount == 0)
	   throw std::runtime_error("No GPUs with Vulkan support");
	   std::vector<VkPhysicalDevice> devices(deviceCount);
	   vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	   std::vector<const char*> deviceExtensions;
	   deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	// Find the most suitable physical device
	m_gpu = PickDevice(devices, m_surface, deviceExtensions);

	// Create the logical device
	QueueFamilyIndices indices                      = FindQueueFamilies(m_gpu, m_surface);
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos {};
	std::set<uint32_t> uniqueQueueFamilies          = {indices.graphicsFamily.value(), indices.presentationFamily.value()};

	for(auto queue : uniqueQueueFamilies)
	{

	float queuePriority                             = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo {};
	queueCreateInfo.sType                           = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex                = queue;
	queueCreateInfo.queueCount                      = 1;
	queueCreateInfo.pQueuePriorities                = &queuePriority;
	queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo deviceCreateInfo {};
	deviceCreateInfo.sType                          = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos              = queueCreateInfos.data();
	deviceCreateInfo.queueCreateInfoCount           = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pEnabledFeatures               = &deviceFeatures;
	deviceCreateInfo.ppEnabledExtensionNames        = deviceExtensions.data();
	deviceCreateInfo.enabledExtensionCount          = static_cast<uint32_t>(deviceExtensions.size());

	VK_CHECK(vkCreateDevice(m_gpu, &deviceCreateInfo, nullptr, &m_device), "Failed to create logical device");

	// retrieve the handle to the queue created with the logical device
	vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_device, indices.presentationFamily.value(), 0, &m_presentQueue); */

	std::vector<const char*> deviceExtensions;
	deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	m_gpu = PickDevice(devices, m_surface, deviceExtensions);

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(m_gpu, &deviceProperties);
	LOG_TRACE("Picked device: {0}", deviceProperties.deviceName);

	m_msaaSamples = GetMaxUsableSampleCount(m_gpu);

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
	deviceFeatures.depthBounds = VK_TRUE; //doesnt work on my surface 2017
	// these are for dynamic descriptor indexing
	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {};
	descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
	descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;

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

void RendererSystem::CreateSwapchain()
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
	m_swapchainImages.resize(imageCount);
	VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data()), "Failed to get swapchain images");
	m_swapchainExtent = extent;
	m_swapchainImageFormat = surfaceFormat.format;


	m_swapchainImageViews.resize(m_swapchainImages.size());

	for (size_t i = 0; i < m_swapchainImageViews.size(); i++)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType	= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image    = m_swapchainImages[i];
		createInfo.format   = m_swapchainImageFormat;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel    = 0;
		createInfo.subresourceRange.levelCount      = 1;
		createInfo.subresourceRange.baseArrayLayer  = 0;
		createInfo.subresourceRange.layerCount      = 1;


		VK_CHECK(vkCreateImageView(m_device, &createInfo, nullptr, &m_swapchainImageViews[i]), "Failed to create swapchain image views");
	}

}

void RendererSystem::CreateRenderPass()
{



	// Main render pass
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = m_swapchainImageFormat;
	colorAttachment.samples = m_msaaSamples;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if(m_msaaSamples != VK_SAMPLE_COUNT_1_BIT)
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	else
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = VK_FORMAT_D32_SFLOAT;
	depthAttachment.samples = m_msaaSamples;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	VkAttachmentDescription colorAttachmentResolve = {}; // To "combine" all the samples from the msaa attachment
	colorAttachmentResolve.format = m_swapchainImageFormat;
	colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorRef = {};
	colorRef.attachment = 0;
	colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


	VkAttachmentReference depthRef = {};
	depthRef.attachment = 1;
	depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	VkAttachmentReference colorResolveRef = {};
	colorResolveRef.attachment = 2;
	colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 1;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pColorAttachments = &colorRef;
	subpass.pDepthStencilAttachment = &depthRef;
	if(m_msaaSamples != VK_SAMPLE_COUNT_1_BIT)
		subpass.pResolveAttachments = &colorResolveRef;

	std::vector<VkAttachmentDescription> attachments;
	attachments.push_back(colorAttachment);
	attachments.push_back(depthAttachment);
	if(m_msaaSamples != VK_SAMPLE_COUNT_1_BIT)
		attachments.push_back(colorAttachmentResolve);

	VkRenderPassCreateInfo createInfo = {};
	createInfo.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount  = static_cast<uint32_t>(attachments.size());
	createInfo.pAttachments     = attachments.data();
	createInfo.subpassCount     = 1;
	createInfo.pSubpasses       = &subpass;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	createInfo.dependencyCount = 1;
	createInfo.pDependencies = &dependency;

	VK_CHECK(vkCreateRenderPass(m_device, &createInfo, nullptr, &m_renderPass), "Failed to create render pass");

	// Render pass for the depth prepass
	VkAttachmentDescription prePassDepthAttachment = {};
	prePassDepthAttachment.format = VK_FORMAT_D32_SFLOAT;
	prePassDepthAttachment.samples = m_msaaSamples;
	prePassDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	prePassDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	prePassDepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	prePassDepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	prePassDepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	prePassDepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	VkAttachmentReference prePassDepthRef = {};
	prePassDepthRef.attachment = 0;
	prePassDepthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference prePassColorResolveRef = {};
	prePassColorResolveRef.attachment = 1;
	prePassColorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription prePassSubpass = {};
	prePassSubpass.colorAttachmentCount = 0;
	prePassSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	prePassSubpass.pDepthStencilAttachment = &prePassDepthRef;
	if(m_msaaSamples != VK_SAMPLE_COUNT_1_BIT)
		prePassSubpass.pResolveAttachments = &prePassColorResolveRef;

	std::vector<VkAttachmentDescription> prePassAttachments;
	prePassAttachments.push_back(prePassDepthAttachment);
	if(m_msaaSamples != VK_SAMPLE_COUNT_1_BIT)
		attachments.push_back(colorAttachmentResolve);

	VkRenderPassCreateInfo prePassCreateInfo = {};
	prePassCreateInfo.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	prePassCreateInfo.attachmentCount  = (uint32_t)prePassAttachments.size();
	prePassCreateInfo.pAttachments     = prePassAttachments.data();
	prePassCreateInfo.subpassCount     = 1;
	prePassCreateInfo.pSubpasses       = &prePassSubpass;


	VkSubpassDependency prePassDependency = {};
	prePassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	prePassDependency.dstSubpass = 0;
	prePassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	prePassDependency.srcAccessMask = 0;
	prePassDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	prePassDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

	prePassCreateInfo.dependencyCount = 1;
	prePassCreateInfo.pDependencies = &prePassDependency;

	VK_CHECK(vkCreateRenderPass(m_device, &prePassCreateInfo, nullptr, &m_prePassRenderPass), "Failed to create pre pass render pass");
}

void RendererSystem::CreatePipeline()
{

	// Main graphics pipeline

	auto vsModule = m_pipelines["base"].shaders[0].GetShaderModule();
	auto fsModule = m_pipelines["base"].shaders[1].GetShaderModule();

	VkPipelineShaderStageCreateInfo vertex = {};
	vertex.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertex.stage    = VK_SHADER_STAGE_VERTEX_BIT;
	vertex.pName    = "main";
	vertex.module   = vsModule;

	VkPipelineShaderStageCreateInfo fragment = {};
	fragment.sType	  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragment.stage    = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragment.pName    = "main";
	fragment.module   = fsModule;

	VkPipelineShaderStageCreateInfo stages[] = {vertex, fragment};

	auto bindingDescription = GetVertexBindingDescription();
	auto attribDescriptions = GetVertexAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInput = {}; // vertex info hardcoded for the moment
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &bindingDescription;
	vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribDescriptions.size());
	vertexInput.pVertexAttributeDescriptions = attribDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo assembly = {};
	assembly.sType	  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkViewport viewport = {};
	viewport.width  = (float)m_swapchainExtent.width;
	viewport.height = -(float)m_swapchainExtent.height;
	viewport.x = 0.f;
	viewport.y = (float)m_swapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = m_swapchainExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pViewports = &viewport;
	viewportState.viewportCount = 1;
	viewportState.pScissors = &scissor;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.depthClampEnable = false;
	rasterizer.rasterizerDiscardEnable = false;
	rasterizer.lineWidth = 1.0f;
	rasterizer.depthBiasEnable = false;

	VkPipelineMultisampleStateCreateInfo multisample = {};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.sampleShadingEnable = VK_TRUE;
	multisample.minSampleShading = 0.2f; // closer to 1 is smoother
	multisample.rasterizationSamples = m_msaaSamples;


	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
	colorBlendAttachment.blendEnable = true;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlend = {};
	colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.logicOpEnable = false;
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments = &colorBlendAttachment;

	VkPipelineDepthStencilStateCreateInfo depthStencil  = {};
	depthStencil.sType  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_FALSE; // false since we have a depth prepass
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // not OP_LESS because we have a depth prepass
	depthStencil.depthBoundsTestEnable = VK_TRUE;
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;
	depthStencil.stencilTestEnable = VK_FALSE;


	VkPushConstantRange pcRange = {};
	pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pcRange.offset = 0;
	pcRange.size = sizeof(glm::mat4); // we will only send the view projection matrix for now

	/*VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH };
	  VkPipelineDynamicStateCreateInfo dynamicState = {};
	  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	  dynamicState.dynamicStateCount = 2;
	  dynamicState.pDynamicStates = dynamicStates;
	  */
	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = 1;
	layoutCreateInfo.pSetLayouts = &m_pipelines["base"].descriptorSetLayout;
	layoutCreateInfo.pushConstantRangeCount = 1;
	layoutCreateInfo.pPushConstantRanges = &pcRange;

	VK_CHECK(vkCreatePipelineLayout(m_device, &layoutCreateInfo, nullptr, &m_pipelineLayout), "Failed to create pipeline layout");


	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = stages;
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &assembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisample;
	pipelineInfo.pColorBlendState = &colorBlend;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT; // for the prepass pipeline
	//pipelineInfo.pDynamicState = &dynamicState;

	pipelineInfo.layout = m_pipelineLayout;
	pipelineInfo.renderPass = m_renderPass;
	pipelineInfo.subpass = 0;

	VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline), "Failed to create graphics pipeline");

	m_pipelines["base"].layout = m_pipelineLayout;
	m_pipelines["base"].pipeline = m_graphicsPipeline;
	m_pipelines["base"].isMaterial = true;


	// Depth prepass pipeline

	LOG_WARN("depth");
	auto prePassVsModule = m_pipelines["depth"].shaders[0].GetShaderModule();
	VkPipelineShaderStageCreateInfo prePassVertex = {};
	prePassVertex.sType	   = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	prePassVertex.stage    = VK_SHADER_STAGE_VERTEX_BIT;
	prePassVertex.pName    = "main";
	prePassVertex.module   = prePassVsModule;

	VkPipelineShaderStageCreateInfo prePassStages[] = {prePassVertex};

	VkPipelineLayoutCreateInfo prePassLayoutCreateInfo = {};
	prePassLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	prePassLayoutCreateInfo.setLayoutCount = 1;
	prePassLayoutCreateInfo.pSetLayouts = &m_pipelines["depth"].descriptorSetLayout;
	prePassLayoutCreateInfo.pushConstantRangeCount = 1;
	prePassLayoutCreateInfo.pPushConstantRanges = &pcRange;

	VK_CHECK(vkCreatePipelineLayout(m_device, &prePassLayoutCreateInfo, nullptr, &m_prePassPipelineLayout), "Failed to create pre pass pipeline layout");

	VkPipelineDepthStencilStateCreateInfo prePassDepthStencil  = {};
	prePassDepthStencil.sType  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	prePassDepthStencil.depthTestEnable = VK_TRUE;
	prePassDepthStencil.depthWriteEnable = VK_TRUE;
	prePassDepthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	prePassDepthStencil.depthBoundsTestEnable = VK_TRUE;
	prePassDepthStencil.minDepthBounds = 0.0f;
	prePassDepthStencil.maxDepthBounds = 1.0f;
	prePassDepthStencil.stencilTestEnable = VK_FALSE;


	VkGraphicsPipelineCreateInfo prePassPipelineInfo = {};
	prePassPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	prePassPipelineInfo.stageCount = 1;
	prePassPipelineInfo.pStages = prePassStages;
	prePassPipelineInfo.pVertexInputState = &vertexInput;
	prePassPipelineInfo.pInputAssemblyState = &assembly;
	prePassPipelineInfo.pViewportState = &viewportState;
	prePassPipelineInfo.pRasterizationState = &rasterizer;
	prePassPipelineInfo.pMultisampleState = &multisample;
	prePassPipelineInfo.pColorBlendState = nullptr;
	prePassPipelineInfo.pDepthStencilState = &prePassDepthStencil;
	prePassPipelineInfo.basePipelineHandle = m_graphicsPipeline;
	prePassPipelineInfo.basePipelineIndex = -1;
	prePassPipelineInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT; //make it a child of the main pipeline

	prePassPipelineInfo.layout = m_prePassPipelineLayout;
	prePassPipelineInfo.renderPass = m_prePassRenderPass;
	prePassPipelineInfo.subpass = 0;

	VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &prePassPipelineInfo, nullptr, &m_prePassPipeline), "Failed to create pre pass pipeline");

	m_pipelines["depth"].layout = m_prePassPipelineLayout;
	m_pipelines["depth"].pipeline = m_prePassPipeline;




}

void RendererSystem::CreateFramebuffers()
{
	m_swapchainFramebuffers.resize(m_swapchainImageViews.size());
	for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
		{
			std::vector<VkImageView> attachments;
			if(m_msaaSamples != VK_SAMPLE_COUNT_1_BIT)
			{
				attachments.push_back(m_colorImage->GetImageView());
				attachments.push_back(m_depthImage->GetImageView());
				attachments.push_back(m_swapchainImageViews[i]);
			}
			else
			{
				attachments.push_back(m_swapchainImageViews[i]);
				attachments.push_back(m_depthImage->GetImageView());
			}

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass		= m_renderPass;
			framebufferInfo.width			= m_swapchainExtent.width;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments	= attachments.data();
			framebufferInfo.height			= m_swapchainExtent.height;
			framebufferInfo.layers			= 1;

			VK_CHECK(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapchainFramebuffers[i]), "Failed to create framebuffer");
		}
		// depth pass framebuffers
	}
	{
		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass		= m_prePassRenderPass;
		framebufferInfo.width			= m_swapchainExtent.width;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments	= &m_depthImage->GetImageView();;
		framebufferInfo.height			= m_swapchainExtent.height;
		framebufferInfo.layers			= 1;


		VK_CHECK(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_depthFramebuffers), "Failed to create depth framebuffer");
	}



}

void RendererSystem::CreateCommandPool()
{
	QueueFamilyIndices families = FindQueueFamilies(m_gpu, m_surface);
	VkCommandPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = families.graphicsFamily.value();
	createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VK_CHECK(vkCreateCommandPool(m_device, &createInfo, nullptr, &m_commandPool), "Failed to create command pool");
}

void RendererSystem::CreateCommandBuffers()
{
	for(size_t i = 0; i < m_swapchainImages.size(); i++)
	{
		m_mainCommandBuffers.push_back(CommandBuffer());
		m_mainCommandBuffers[i].Allocate(m_device, m_commandPool);
		m_depthCommandBuffers.push_back(CommandBuffer());
		m_depthCommandBuffers[i].Allocate(m_device, m_commandPool);
	}
}

void RendererSystem::CreateSyncObjects()
{
	m_imageAvailable.resize(MAX_FRAMES_IN_FLIGHT);
	m_renderFinished.resize(MAX_FRAMES_IN_FLIGHT);
	m_prePassFinished.resize(MAX_FRAMES_IN_FLIGHT);
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
		VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_prePassFinished[i]), "Failed to create semaphore");
	}

}

void RendererSystem::CreateDescriptorSetLayout()
{
	Shader vs("base", VK_SHADER_STAGE_VERTEX_BIT);
	Shader fs("base", VK_SHADER_STAGE_FRAGMENT_BIT);

	m_pipelines["base"].shaders.push_back(vs);
	m_pipelines["base"].shaders.push_back(fs);
	Shader prePassVs("depth", VK_SHADER_STAGE_VERTEX_BIT);
	m_pipelines["depth"].shaders.push_back(prePassVs);

	for(auto& [_, pipeline] : m_pipelines)
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		for(auto& shader : pipeline.shaders)
		{
			for(auto& [name, bufferInfo] : shader.m_uniformBuffers)
			{
				VkDescriptorSetLayoutBinding layoutBinding  = {};
				layoutBinding.binding                       = bufferInfo.binding;
				layoutBinding.descriptorType                = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				layoutBinding.descriptorCount               = bufferInfo.count;
				layoutBinding.stageFlags                    = bufferInfo.stage;
				layoutBinding.pImmutableSamplers            = nullptr; // this is for texture samplers

				bindings.push_back(layoutBinding);

			}
			for(auto& [name, textureInfo] : shader.m_Textures)
			{
				VkDescriptorSetLayoutBinding samplerLayoutBinding  = {};
				samplerLayoutBinding.binding                       = textureInfo.binding;
				samplerLayoutBinding.descriptorType                = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				if(pipeline.isMaterial && (textureInfo.stage == VK_SHADER_STAGE_FRAGMENT_BIT) && pipeline.variableDescCount) // TODO dont hardcode the stage
					samplerLayoutBinding.descriptorCount           = textureInfo.count * OBJECTS_PER_DESCRIPTOR_CHUNK;
				else
					samplerLayoutBinding.descriptorCount           = textureInfo.count;
				samplerLayoutBinding.stageFlags                    = textureInfo.stage;
				samplerLayoutBinding.pImmutableSamplers            = nullptr;

				bindings.push_back(samplerLayoutBinding);

			}
		}
		VkDescriptorSetLayoutCreateInfo createInfo  = {};
		createInfo.sType                            = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount                     = static_cast<uint32_t>(bindings.size());
		createInfo.pBindings                        = bindings.data();
		createInfo.pNext = nullptr;
		if(pipeline.variableDescCount)
		{
			VkDescriptorSetLayoutBindingFlagsCreateInfo flags = {};
			flags.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
			flags.bindingCount = bindings.size();
			std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size() - 1, 0);
			bindingFlags.push_back(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);
			flags.pBindingFlags = bindingFlags.data();
			createInfo.pNext							= &flags;
		}
		VK_CHECK(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &pipeline.descriptorSetLayout), "Failed to create descriptor set layout");
	}



}

void RendererSystem::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 3> poolSizes   = {};
	poolSizes[0].type                               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount                    = static_cast<uint32_t>(m_swapchainImages.size());
	poolSizes[1].type                               = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount                    = static_cast<uint32_t>(m_swapchainImages.size())+1; // +1 for imgui
	poolSizes[2].type								= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	poolSizes[2].descriptorCount					= 100; //static_cast<uint32_t>(m_swapchainImages.size());;


	VkDescriptorPoolCreateInfo createInfo   = {};
	createInfo.sType                        = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.poolSizeCount                = static_cast<uint32_t>(poolSizes.size());
	createInfo.pPoolSizes                   = poolSizes.data();
	createInfo.maxSets                      = 100;
	VK_CHECK(vkCreateDescriptorPool(m_device, &createInfo, nullptr, &m_descriptorPool), "Failed to create descriptor pool");
}

void RendererSystem::CreateDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> globalLayouts(m_swapchainImages.size(), m_globalLayout);

	VkDescriptorSetAllocateInfo allocInfo = {};
	/*
	   allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	   allocInfo.descriptorPool		= m_descriptorPool;
	   allocInfo.descriptorSetCount	= static_cast<uint32_t>(globalLayouts.size());
	   allocInfo.pSetLayouts			= globalLayouts.data();

	   m_globalDescSets.resize(m_swapchainImages.size());
	   VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, m_globalDescSets.data()), "Failed to allocate global descriptor sets");
	   */

	// descriptors for the different materials
	for(auto& [pipelineName,pipelineInfo] : m_pipelines)
	{
		std::vector<VkDescriptorSetLayout> materialLayouts;
		for (int i = 0; i < m_swapchainImages.size(); ++i)
		{
			materialLayouts.push_back(pipelineInfo.descriptorSetLayout);
		}
		allocInfo = {};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool		= m_descriptorPool;
		allocInfo.descriptorSetCount	= static_cast<uint32_t>(materialLayouts.size());
		allocInfo.pSetLayouts			= materialLayouts.data();

		m_descriptorSets[pipelineName + "0"].resize(m_swapchainImages.size()); //materials are in set 1 TODO
		VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets[pipelineName + "0"].data()), "Failed to allocate descriptor sets");
	}

	for(auto& [pipelineName,pipelineInfo] : m_pipelines)
	{
		for (int i = 0; i < m_swapchainImages.size(); ++i)
		{
			for(auto& shader : pipelineInfo.shaders)
			{
				for(auto& [name, bufferInfo] : shader.m_uniformBuffers)
				{
					VkDescriptorBufferInfo bufferI = {};
					bufferI.buffer	= m_ubAllocators[pipelineName + name]->GetBuffer(0);
					bufferI.offset	= 0;
					bufferI.range	= VK_WHOLE_SIZE;

					VkWriteDescriptorSet writeDS = {};
					writeDS.sType	= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writeDS.dstSet	= m_descriptorSets[pipelineName + std::to_string(bufferInfo.set)][i];
					writeDS.dstBinding	= bufferInfo.binding;
					writeDS.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
					writeDS.descriptorCount = 1;
					writeDS.pBufferInfo	= &bufferI;

					vkUpdateDescriptorSets(m_device, 1, &writeDS, 0, nullptr);
				}
				for(auto& [name, textureInfo] : shader.m_Textures)
				{
					VkDescriptorImageInfo imageInfo = {};
					imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					imageInfo.sampler = m_sampler;
					imageInfo.imageView = m_texture->GetImageView();

					VkWriteDescriptorSet writeDS = {};
					writeDS.sType	= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writeDS.dstBinding = textureInfo.binding;
					writeDS.descriptorType	= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					writeDS.dstArrayElement = 0;
					writeDS.dstSet	= m_descriptorSets[pipelineName + std::to_string(textureInfo.set)][i];

					if(pipelineInfo.isMaterial && (textureInfo.stage == VK_SHADER_STAGE_FRAGMENT_BIT) && pipelineInfo.variableDescCount) // TODO dont hardcode the stage
						writeDS.descriptorCount               = textureInfo.count * OBJECTS_PER_DESCRIPTOR_CHUNK;
					else
						writeDS.descriptorCount               = textureInfo.count;
					writeDS.pImageInfo = &imageInfo;

					vkUpdateDescriptorSets(m_device, 1, &writeDS, 0, nullptr);


				}
			}
		}
	}

}

void RendererSystem::CreateUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	m_uniformBuffers.reserve(m_swapchainImages.size());

	for(size_t i = 0; i < m_swapchainImages.size(); i++)
	{
		m_uniformBuffers.push_back(std::make_unique<UniformBuffer>(m_gpu, m_device, bufferSize));
	}

}

void RendererSystem::CreateTexture()
{
	m_texture = std::make_unique<Texture>("./textures/chalet.jpg", m_gpu, m_device, m_commandPool, m_graphicsQueue);
}

void RendererSystem::CreateSampler()
{
	VkSamplerCreateInfo ci = {};
	ci.sType	= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	ci.magFilter = VK_FILTER_LINEAR;
	ci.minFilter = VK_FILTER_LINEAR;
	ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	ci.anisotropyEnable = VK_TRUE;
	ci.maxAnisotropy = 16;
	ci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	ci.unnormalizedCoordinates = VK_FALSE; // this should always be false because UV coords are in [0,1) not in [0, width),etc...
	ci.compareEnable = VK_FALSE; // this is used for percentage closer filtering for shadow maps
	ci.compareOp     = VK_COMPARE_OP_ALWAYS;
	ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	ci.mipLodBias = 0.0f;
	ci.minLod = 0.0f;
	ci.maxLod = m_texture->GetMipLevels();

	VK_CHECK(vkCreateSampler(m_device, &ci, nullptr, &m_sampler), "Failed to create texture sampler");

}

void RendererSystem::CreateColorResources()
{
	m_colorImage = std::make_unique<Image>(m_gpu, m_device, m_swapchainExtent.width, m_swapchainExtent.height, m_swapchainImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT, m_msaaSamples);
}

void RendererSystem::CreateDepthResources()
{
	m_depthImage = std::make_unique<Image>(m_gpu, m_device, m_swapchainExtent.width, m_swapchainExtent.height,
			VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, m_msaaSamples);
}

void RendererSystem::Update(float dt)
{
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


	m_debugUI->SetupFrame(imageIndex, 0, m_swapchainFramebuffers[imageIndex]);	//subpass is 0 because we only have one subpass for now

	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();



	int i = -1;

	// depth prepass
	{
		m_depthCommandBuffers[imageIndex].Begin(0);

		VkRenderPassBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		beginInfo.renderPass = m_prePassRenderPass;
		beginInfo.framebuffer = m_depthFramebuffers;
		beginInfo.renderArea.offset = {0,0};
		beginInfo.renderArea.extent = m_swapchainExtent;

		VkClearValue clearValue;
		clearValue.depthStencil = {1.0f, 0};
		beginInfo.clearValueCount = 1;
		beginInfo.pClearValues = &clearValue;

		vkCmdBeginRenderPass(m_depthCommandBuffers[imageIndex].GetCommandBuffer(), &beginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		std::vector<VkCommandBuffer> secondaryCbs;
		ComponentManager* cm = m_ecsEngine->componentManager;


		// TODO: make something better for the cameras, now it only takes the first camera that was added to the componentManager
		// also a single camera has a whole chunk of memory which isnt good if we only use 1 camera per game
		Camera camera = *(*(cm->begin<Camera>()));
		Transform* cameraTransform = cm->GetComponent<Transform>(camera.GetOwner());

		for (auto&& comp : *cm->GetComponents<Renderable>())
		{
			VkCommandBufferInheritanceInfo inheritanceInfo = {};
			inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inheritanceInfo.renderPass = m_prePassRenderPass;
			inheritanceInfo.subpass = 0;
			inheritanceInfo.framebuffer = m_depthFramebuffers;


			comp->prePassCommandBuffers[imageIndex].Begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, inheritanceInfo);


			glm::mat4 vp = camera.GetProjection() * glm::toMat4(glm::conjugate(cameraTransform->wRotation)) * glm::translate(cameraTransform->wPosition);
			UniformBufferObject ubo{};
			Transform* t = cm->GetComponent<Transform>(comp->GetOwner());
			ubo.model =  glm::translate(t->wPosition) * glm::rotate(glm::mat4(1.0f), i * time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			m_uniformBuffers[imageIndex]->Update(&ubo);

			VkCommandBuffer cb = comp->prePassCommandBuffers[imageIndex].GetCommandBuffer();
			vkCmdPushConstants(cb, m_prePassPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &vp);
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_prePassPipeline);

			comp->vertexBuffer.Bind(comp->prePassCommandBuffers[imageIndex]);
			comp->indexBuffer.Bind(comp->prePassCommandBuffers[imageIndex]);

			// TODO temp
			auto* mat = cm->GetComponent<Material>(comp->GetOwner());
			uint32_t offset;
			m_ubAllocators["depthUniformBufferObject"]->GetBufferAndOffset(t->ub_id, offset);

			m_ubAllocators["depthUniformBufferObject"]->UpdateBuffer(t->ub_id, &ubo);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets["depth" + std::to_string(0)][imageIndex], 1, &offset);
			vkCmdDrawIndexed(cb, static_cast<uint32_t>(comp->indexBuffer.GetSize() / sizeof(uint32_t)) , 1, 0, 0, 0); // TODO: make the size calculation better (not hardcoded for uint32_t)

			comp->prePassCommandBuffers[imageIndex].End();


			secondaryCbs.push_back(cb);
			i *= -1;
		}

		vkCmdExecuteCommands(m_depthCommandBuffers[imageIndex].GetCommandBuffer(), static_cast<uint32_t>(secondaryCbs.size()), secondaryCbs.data());
		vkCmdEndRenderPass(m_depthCommandBuffers[imageIndex].GetCommandBuffer());


		//m_depthCommandBuffer.Submit(m_graphicsQueue, VK_NULL_HANDLE, VK_NULL_HANDLE, m_prePassFinished, VK_NULL_HANDLE);
		m_depthCommandBuffers[imageIndex].End();
	}

	m_mainCommandBuffers[imageIndex].Begin(0);

	VkRenderPassBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderPass = m_renderPass;
	beginInfo.framebuffer = m_swapchainFramebuffers[imageIndex];
	beginInfo.renderArea.offset = {0,0};
	beginInfo.renderArea.extent = m_swapchainExtent;

	std::array<VkClearValue, 2> clearValues = {};
	clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
	clearValues[1].depthStencil = {1.0f, 0};
	beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	beginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(m_mainCommandBuffers[imageIndex].GetCommandBuffer(), &beginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	std::vector<VkCommandBuffer> secondaryCbs;
	ComponentManager* cm = m_ecsEngine->componentManager;


	// TODO: make something better for the cameras, now it only takes the first camera that was added to the componentManager
	// also a single camera has a whole chunk of memory which isnt good if we only use 1 camera per game
	Camera camera = *(*(cm->begin<Camera>()));
	Transform* cameraTransform = cm->GetComponent<Transform>(camera.GetOwner());

	for (auto&& comp : *cm->GetComponents<Renderable>())
	{
		VkCommandBufferInheritanceInfo inheritanceInfo = {};
		inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritanceInfo.renderPass = m_renderPass;
		inheritanceInfo.subpass = 0;
		inheritanceInfo.framebuffer = m_swapchainFramebuffers[imageIndex];

		comp->commandBuffers[imageIndex].Begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, inheritanceInfo);


		glm::mat4 vp = camera.GetProjection() * glm::toMat4(glm::conjugate(cameraTransform->wRotation)) * glm::translate(cameraTransform->wPosition);

		UniformBufferObject ubo{};
		Transform* t = cm->GetComponent<Transform>(comp->GetOwner());
		ubo.model =  glm::translate(t->wPosition) * glm::rotate(glm::mat4(1.0f), i * time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

		m_uniformBuffers[imageIndex]->Update(&ubo);
		VkCommandBuffer cb = comp->commandBuffers[imageIndex].GetCommandBuffer();
		vkCmdPushConstants(cb, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &vp);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

		comp->vertexBuffer.Bind(comp->commandBuffers[imageIndex]);
		comp->indexBuffer.Bind(comp->commandBuffers[imageIndex]);

		auto* mat = cm->GetComponent<Material>(comp->GetOwner());
		uint32_t offset;
		m_ubAllocators[mat->shaderName + "UniformBufferObject"]->GetBufferAndOffset(t->ub_id, offset);

		m_ubAllocators[mat->shaderName + "UniformBufferObject"]->UpdateBuffer(t->ub_id, &ubo);

		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[mat->shaderName + std::to_string(0)][imageIndex], 1, &offset);
		vkCmdDrawIndexed(cb, static_cast<uint32_t>(comp->indexBuffer.GetSize() / sizeof(uint32_t)) , 1, 0, 0, 0); // TODO: make the size calculation better (not hardcoded for uint32_t)

		comp->commandBuffers[imageIndex].End();


		secondaryCbs.push_back(cb);
		i *= -1;
	}

	secondaryCbs.push_back(m_debugUI->GetCommandBuffer(imageIndex));

	vkCmdExecuteCommands(m_mainCommandBuffers[imageIndex].GetCommandBuffer(), static_cast<uint32_t>(secondaryCbs.size()), secondaryCbs.data());
	vkCmdEndRenderPass(m_mainCommandBuffers[imageIndex].GetCommandBuffer());
	m_mainCommandBuffers[imageIndex].End();

	//	m_mainCommandBuffers[imageIndex].Submit(m_graphicsQueue, m_prePassFinished, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, m_renderFinished[m_currentFrame], m_inFlightFences[m_currentFrame]);
	VkCommandBuffer prepassCb = m_depthCommandBuffers[imageIndex].GetCommandBuffer();
	VkCommandBuffer mainCb = m_mainCommandBuffers[imageIndex].GetCommandBuffer();

	VkSubmitInfo submitInfo[2] = {};
	submitInfo[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo[0].commandBufferCount = 1;
	submitInfo[0].pCommandBuffers = &prepassCb;
	submitInfo[0].signalSemaphoreCount = 1;
	submitInfo[0].pSignalSemaphores = &m_prePassFinished[m_currentFrame];

	VkPipelineStageFlags wait[2] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore waitSemaphores[2] = {m_prePassFinished[m_currentFrame], m_imageAvailable[m_currentFrame]};
	submitInfo[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo[1].commandBufferCount = 1;
	submitInfo[1].pCommandBuffers = &mainCb;
	submitInfo[1].waitSemaphoreCount = 2;
	submitInfo[1].pWaitSemaphores = waitSemaphores;
	submitInfo[1].pWaitDstStageMask = wait;
	submitInfo[1].signalSemaphoreCount = 1;
	submitInfo[1].pSignalSemaphores = &m_renderFinished[m_currentFrame];

	VK_CHECK(vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]), "Failed to reset fence");
	VK_CHECK(vkQueueSubmit(m_graphicsQueue, 2, submitInfo, m_inFlightFences[m_currentFrame]), "Failed to submit command buffers");



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


void RendererSystem::OnMeshComponentAdded(const ComponentAdded<Mesh>* e)
{
	Mesh* mesh = e->component;
	VkDeviceSize vBufferSize = sizeof(mesh->vertices[0]) * mesh->vertices.size();

	// create the vertex and index buffers on the gpu
	Buffer stagingVertexBuffer(m_gpu, m_device, vBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	stagingVertexBuffer.Fill((void*)mesh->vertices.data(), vBufferSize);

	VkDeviceSize iBufferSize = sizeof(mesh->indices[0]) * mesh->indices.size();
	Buffer stagingIndexBuffer(m_gpu, m_device, iBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	stagingIndexBuffer.Fill((void*)mesh->indices.data(), iBufferSize);

	std::vector<CommandBuffer> cbs;
	std::vector<CommandBuffer> prePassCbs;
	for (int i = 0; i < m_swapchainImages.size(); ++i)
	{
		cbs.push_back(CommandBuffer());
		prePassCbs.push_back(CommandBuffer());
		cbs[i].Allocate(m_device, m_commandPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		prePassCbs[i].Allocate(m_device, m_commandPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
	}
	Renderable* comp = m_ecsEngine->componentManager->AddComponent<Renderable>(e->entity,
			cbs,
			prePassCbs,
			Buffer(m_gpu, m_device, vBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
			Buffer(m_gpu, m_device, iBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			);

	stagingVertexBuffer.Copy(&comp->vertexBuffer, vBufferSize, m_graphicsQueue, m_commandPool);

	stagingIndexBuffer.Copy(&comp->indexBuffer, iBufferSize, m_graphicsQueue, m_commandPool);

	stagingVertexBuffer.Free();
	stagingIndexBuffer.Free();

	for (auto& [name, pipeline] : m_pipelines)
	{
		uint32_t id = m_ubAllocators[name + "UniformBufferObject"]->Allocate();
		m_ecsEngine->componentManager->GetComponent<Transform>(e->entity)->ub_id = id;
	}

}

void RendererSystem::OnMeshComponentRemoved(const ComponentRemoved<Mesh>* e)
{
	m_ecsEngine->componentManager->RemoveComponent<Renderable>(e->entity);
}

void RendererSystem::OnMaterialComponentAdded(const ComponentAdded<Material>* e)
{
	m_ecsEngine->componentManager->Sort<Material>([](Material* lhs, Material* rhs)
			{
				return lhs->shaderName <= rhs->shaderName;
			});
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
	std::cout << "Device: " << deviceProperties.deviceName  << " score: " << score << std::endl;
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
	// Select prefix depending on flags passed to the callback
	std::string prefix("");

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
		prefix = "VERBOSE: ";
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		prefix = "INFO: ";
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		prefix = "WARNING: ";
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		prefix = "ERROR: ";
	}
	std::cerr << prefix.c_str() << "\n";
	std::cerr << "\t" << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
	std::cerr << "\t" << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
	std::cerr << "\t" << "message         = <" << pCallbackData->pMessage << ">\n";

	if (0 < pCallbackData->queueLabelCount)
	{
		std::cerr << "\t" << "Queue Labels:\n";
		for (uint8_t i = 0; i < pCallbackData->queueLabelCount; i++)
		{
			std::cerr << "\t\t" << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
		}
	}
	if (0 < pCallbackData->cmdBufLabelCount)
	{
		std::cerr << "\t" << "CommandBuffer Labels:\n";
		for (uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++)
		{
			std::cerr << "\t\t" << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
		}
	}
	if (0 < pCallbackData->objectCount)
	{
		std::cerr << "\t" << "Objects:\n";
		for (uint8_t i = 0; i < pCallbackData->objectCount; i++)
		{
			std::cerr << "\t\t" << "Object " << i << "\n";
			std::cerr << "\t\t\t" << "objectType   = " << pCallbackData->pObjects[i].objectType << "\n";
			std::cerr << "\t\t\t" << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
			if (pCallbackData->pObjects[i].pObjectName)
			{
				std::cerr << "\t\t\t" << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
			}
		}
	}
	std::cerr << "\n\n";
	return VK_TRUE;
}
