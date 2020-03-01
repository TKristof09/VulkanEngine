#include "Renderer.hpp"
#include <sstream>
#include <optional>
#include <set>
#include <map>
#include <limits>

#include "Shader.hpp"

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
    vk::SurfaceCapabilitiesKHR            capabilities;
    std::vector<vk::SurfaceFormatKHR>     formats;
    std::vector<vk::PresentModeKHR>       presentModes;
};

std::vector<const char *> GetExtensions();
vk::PhysicalDevice PickDevice(const std::vector<vk::PhysicalDevice>& devices, vk::SurfaceKHR surface, const std::vector<const char*>& deviceExtensions);
int RateDevice(vk::PhysicalDevice device, vk::SurfaceKHR surface, const std::vector<const char*>& deviceExtensions);
QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);
SwapchainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);
vk::SurfaceFormatKHR ChooseSwapchainFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
vk::PresentModeKHR ChooseSwapchainPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
vk::Extent2D ChooseSwapchainExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);

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
    m_graphicsQueue = m_device.getQueue(families.graphicsFamily.value(), 0);
    m_presentQueue  = m_device.getQueue(families.presentationFamily.value(), 0);

}

void Renderer::CreateSwapchain()
{
    SwapchainSupportDetails details = QuerySwapChainSupport(m_gpu, m_surface);

    auto surfaceFormat = ChooseSwapchainFormat(details.formats);
    auto extent = ChooseSwapchainExtent(details.capabilities, m_window->GetWindow());
    auto presentMode = ChooseSwapchainPresentMode(details.presentModes);

    vk::SwapchainCreateInfoKHR createInfo;
    createInfo.surface          = m_surface;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.minImageCount    = details.capabilities.minImageCount + 1;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.presentMode      = presentMode;
    createInfo.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;

    QueueFamilyIndices indices        = FindQueueFamilies(m_gpu, m_surface);
    uint32_t queueFamilyIndices[]       = {indices.graphicsFamily.value(), indices.presentationFamily.value()};

    if (indices.graphicsFamily != indices.presentationFamily)
    {
        createInfo.imageSharingMode         = vk::SharingMode::eConcurrent; // if the two queues are different then we need to share between them
        createInfo.queueFamilyIndexCount    = 2;
        createInfo.pQueueFamilyIndices      = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode         = vk::SharingMode::eExclusive; // this is the better performance
        createInfo.queueFamilyIndexCount    = 0; // Optional
        createInfo.pQueueFamilyIndices      = nullptr; // Optional
    }
    createInfo.preTransform = details.capabilities.currentTransform;
    createInfo.clipped      = VK_TRUE;

    m_swapchain = m_device.createSwapchainKHR(createInfo);

    m_swapchainImages = m_device.getSwapchainImagesKHR(m_swapchain);
    m_swapchainExtent = extent;
    m_swapchainImageFormat = surfaceFormat.format;


    m_swapchainImageViews.resize(m_swapchainImages.size());

    for (size_t i = 0; i < m_swapchainImageViews.size(); i++)
    {
        vk::ImageViewCreateInfo createInfo;
        createInfo.image    = m_swapchainImages[i];
        createInfo.format   = m_swapchainImageFormat;
        createInfo.viewType = vk::ImageViewType::e2D;
        createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        createInfo.subresourceRange.baseMipLevel    = 0;
        createInfo.subresourceRange.levelCount      = 1;
        createInfo.subresourceRange.baseArrayLayer  = 0;
        createInfo.subresourceRange.layerCount      = 1;


        m_swapchainImageViews[i] = m_device.createImageView(createInfo);
    }
}

void Renderer::CreateRenderPass()
{
    vk::AttachmentDescription colorAttachment;
    colorAttachment.format      = m_swapchainImageFormat;
    colorAttachment.loadOp      = vk::AttachmentLoadOp::eClear;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference colorRef;
    colorRef.attachment = 0;
    colorRef.layout     = vk::ImageLayout::eColorAttachmentOptimal;


    vk::SubpassDescription subpass;
    subpass.colorAttachmentCount = 1;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.pColorAttachments = &colorRef;

    vk::RenderPassCreateInfo createInfo;
    createInfo.attachmentCount  = 1;
    createInfo.pAttachments     = &colorAttachment;
    createInfo.subpassCount     = 1;
    createInfo.pSubpasses       = &subpass;


    m_renderPass = m_device.createRenderPass(createInfo);
}

void Renderer::CreatePipeline()
{
    Shader vs("shaders/vert.spv");
    Shader fs("shaders/frag.spv");

    auto vsModule = vs.GetShaderModule(m_device);
    auto fsModule = fs.GetShaderModule(m_device);

    vk::PipelineShaderStageCreateInfo vertex;
    vertex.stage    = vk::ShaderStageFlagBits::eVertex;
    vertex.pName    = "main";
    vertex.module   = vsModule;

    vk::PipelineShaderStageCreateInfo fragment;
    fragment.stage    = vk::ShaderStageFlagBits::eFragment;
    fragment.pName    = "main";
    fragment.module   = fsModule;

    vk::PipelineShaderStageCreateInfo stages[] = {vertex, fragment};

    vk::PipelineVertexInputStateCreateInfo vertexInput; // vertex info hardcoded for the moment

    vk::PipelineInputAssemblyStateCreateInfo assembly;
    assembly.topology = vk::PrimitiveTopology::eTriangleList;

    vk::Viewport viewport;
    viewport.width = m_swapchainExtent.width;
    viewport.height = m_swapchainExtent.height;
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor({0,0}, m_swapchainExtent);

    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.pViewports = &viewport;
    viewportState.viewportCount = 1;
    viewportState.pScissors = &scissor;
    viewportState.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;

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

        SwapchainSupportDetails details = QuerySwapChainSupport(device, surface);
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

SwapchainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface)
{
    SwapchainSupportDetails details;

    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);

    details.formats = device.getSurfaceFormatsKHR(surface);

    details.presentModes = device.getSurfacePresentModesKHR(surface);

    return details;
}


vk::SurfaceFormatKHR ChooseSwapchainFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{

    // if the surface has no preferred format vulkan returns one entity of Vk_FORMAT_UNDEFINED
    // we can then chose whatever we want
    if (availableFormats.size() == 1 && availableFormats[0].format == vk::Format::eUndefined)
    {
        vk::SurfaceFormatKHR format;
        format.colorSpace   = vk::ColorSpaceKHR::eSrgbNonlinear;
        format.format       = vk::Format::eB8G8R8A8Unorm;
        return format;
    }
    for(const auto& format : availableFormats)
    {
        if(format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return format;
    }

    // if we can't find one that we like we could rank them based on how good they are
    // but we will just settle for the first one(apparently in most cases it's okay)
    return availableFormats[0];
}

vk::PresentModeKHR ChooseSwapchainPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
    for(const auto& presentMode : availablePresentModes)
    {
        if(presentMode == vk::PresentModeKHR::eMailbox)
            return presentMode;
    }

    // FIFO is guaranteed to be available
    return vk::PresentModeKHR::eFifo;
}
vk::Extent2D ChooseSwapchainExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
{
    // swap extent is the resolution of the swapchain images

    // if we can set an extent manually the width and height values will be uint32t max
    // else we can't set it so just return it
    if(capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) // () around max to prevent macro expansion by windows.h max macro
        return capabilities.currentExtent;

    // choose an extent within the minImageExtent and maxImageExtent bounds
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
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