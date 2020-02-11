#include "Renderer.hpp"
#include <sstream>
#include <optional>
#include <set>
#include <map>

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentationFamily;

    bool IsComplete() {
        return graphicsFamily.has_value() && presentationFamily.has_value();
    }
};
struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR            capabilities;
    std::vector<vk::SurfaceFormatKHR>     formats;
    std::vector<vk::PresentModeKHR>       presentModes;
};

std::vector<const char *> GetExtensions();
vk::PhysicalDevice PickDevice(const std::vector<vk::PhysicalDevice>& devices, vk::SurfaceKHR surface, const std::vector<const char*>& deviceExtensions);
int RateDevice(vk::PhysicalDevice device, vk::SurfaceKHR surface, const std::vector<const char*>& deviceExtensions);
QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);
SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

VkBool32 debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                         VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData, void * /*pUserData*/);




Renderer::Renderer(std::shared_ptr<Window> window):
m_window(window)
{
    CreateInstance();
    SetupDebugMessenger();
    CreateDevice();
}

Renderer::~Renderer()
{
    m_instance.destroy();
}

void Renderer::CreateInstance()
{
    vk::ApplicationInfo appinfo = {};
    appinfo.pApplicationName = "VulkanEngine";
    appinfo.apiVersion = VK_API_VERSION_1_1;

    vk::InstanceCreateInfo createInfo = {};
    createInfo.pApplicationInfo = &appinfo;

    auto extensions = GetExtensions();
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());

#ifdef VDEBUG
    // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    // Check if this layer is available at instance level
    std::vector<vk::LayerProperties> instanceLayerProperties = vk::enumerateInstanceLayerProperties();

    bool validationLayerPresent = false;
    for (vk::LayerProperties layer : instanceLayerProperties) {
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
#else
    createInfo.ppEnabledLayerNames = nullptr;
    createInfo.enabledLayerCount   = 0;
#endif

    m_instance = vk::createInstance(createInfo);

    VkSurfaceKHR tmpSurface = {};
    if (glfwCreateWindowSurface(m_instance, m_window->GetWindow(), nullptr, &tmpSurface) != VK_SUCCESS) {

    std::runtime_error("failed ot create window surface!");

    }
    m_surface = vk::SurfaceKHR(tmpSurface);

}

void Renderer::SetupDebugMessenger()
{
#ifndef VDEBUG
    return;
#endif
    vk::DebugUtilsMessengerCreateInfoEXT createInfo;
    createInfo.messageSeverity  = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
    createInfo.messageType      = vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    createInfo.pfnUserCallback  = debugUtilsMessengerCallback;

    vk::DispatchLoaderDynamic dl(m_instance, vkGetInstanceProcAddr);

    m_messenger = m_instance.createDebugUtilsMessengerEXT(createInfo, nullptr, dl);

}

void Renderer::CreateDevice()
{
    std::vector<const char*> deviceExtensions;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    m_gpu = PickDevice(m_instance.enumeratePhysicalDevices(), m_surface, deviceExtensions);

    QueueFamilyIndices families = FindQueueFamilies(m_gpu, m_surface);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies          = {families.graphicsFamily.value(), families.presentationFamily.value()};

    for(auto queue : uniqueQueueFamilies)
    {

        float queuePriority                             = 1.0f;
        vk::DeviceQueueCreateInfo queueCreateInfo;
        queueCreateInfo.queueFamilyIndex                = queue;
        queueCreateInfo.queueCount                      = 1;
        queueCreateInfo.pQueuePriorities                = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    vk::PhysicalDeviceFeatures deviceFeatures;
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    vk::DeviceCreateInfo createInfo;
    createInfo.enabledExtensionCount    = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames  = deviceExtensions.data();
    createInfo.pQueueCreateInfos        = queueCreateInfos.data();
    createInfo.queueCreateInfoCount     = static_cast<uint32_t>(queueCreateInfos.size());


    m_device = m_gpu.createDevice(createInfo);
}

// #################################################################################

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

vk::PhysicalDevice PickDevice(const std::vector<vk::PhysicalDevice>& devices, vk::SurfaceKHR surface, const std::vector<const char*>& deviceExtensions)
{
    std::multimap<int, vk::PhysicalDevice> ratedDevices;
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

int RateDevice(vk::PhysicalDevice device, vk::SurfaceKHR surface, const std::vector<const char*>& deviceExtensions)
{

    std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties();

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    bool swapChainGood = false;
    if(requiredExtensions.empty())
    {

        SwapChainSupportDetails details = QuerySwapChainSupport(device, surface);
        swapChainGood = !details.formats.empty() && !details.presentModes.empty();

    }

    vk::PhysicalDeviceProperties deviceProperties = device.getProperties();

    vk::PhysicalDeviceFeatures deviceFeatures = device.getFeatures();

    int score = 0;
    if(deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
        score += 1000;

    score += deviceProperties.limits.maxImageDimension2D;
    bool temp = FindQueueFamilies(device, surface).IsComplete();
    // exemple: if the application cannot function without a geometry shader
    if( !deviceFeatures.geometryShader ||
        !temp ||
        !requiredExtensions.empty() ||
        !swapChainGood ||
        !deviceFeatures.samplerAnisotropy)
    {
        return 0;
    }

    return score;
}

QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface)
{
    QueueFamilyIndices indices;

    std::vector<vk::QueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();
    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if(queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }
        vk::Bool32 presentationSupport = device.getSurfaceSupportKHR(i, surface);
        if(queueFamily.queueCount > 0 && presentationSupport)
        {
            indices.presentationFamily = i;
        }
        if(indices.IsComplete()) {
            break;
        }

        i++;
    }
    return indices;
}

SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface)
{
    SwapChainSupportDetails details;

    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);

    details.formats = device.getSurfaceFormatsKHR(surface);

    details.presentModes = device.getSurfacePresentModesKHR(surface);

    return details;
}

/*
VKAPI_ATTR vk::Bool32 VKAPI_CALL
VulkanDebugCallback(
	VkDebugReportFlagsEXT		flags,
	VkDebugReportObjectTypeEXT	obj_type,
	uint64_t					src_obj,
	size_t						location,
	int32_t						msg_code,
	const char *				layer_prefix,
	const char *				msg,
	void *						user_data
	)
{
	std::ostringstream stream;
	stream << "VKDBG: ";
	if( flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT ) {
		stream << "INFO: ";
	}
	if( flags & VK_DEBUG_REPORT_WARNING_BIT_EXT ) {
		stream << "WARNING: ";
	}
	if( flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT ) {
		stream << "PERFORMANCE: ";
	}
	if( flags & VK_DEBUG_REPORT_ERROR_BIT_EXT ) {
		stream << "ERROR: ";
	}
	if( flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT ) {
		stream << "DEBUG: ";
	}
	stream << "@[" << layer_prefix << "]: ";
	stream << msg << std::endl;
	std::cout << stream.str();

    return false;
} */
VkBool32 debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                         VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData, void * /*pUserData*/)
{
    std::cerr << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) << ": " << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) << ":\n";
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
            std::cerr << "\t\t\t" << "objectType   = " << vk::to_string(static_cast<vk::ObjectType>(pCallbackData->pObjects[i].objectType)) << "\n";
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