#pragma once

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include "Rendering/Queue.hpp"

#define NUM_FRAMES_IN_FLIGHT 2
static const uint32_t NUM_TEXTURE_DESCRIPTORS = 65535;

class Pipeline;
class VulkanContext
{
public:
    static VkInstance GetInstance() { return m_instance; }
    static VkDevice GetDevice() { return m_device; }
    static VkPhysicalDevice GetPhysicalDevice() { return m_gpu; }
    static VkPhysicalDeviceProperties GetPhysicalDeviceProperties() { return m_gpuProperties; }
    static Queue GetGraphicsQueue() { return m_graphicsQueue; }
    static Queue GetTransferQueue() { return m_transferQueue; }
    static Queue GetComputeQueue() { return m_computeQueue; }
    static VkCommandPool GetGraphicsCommandPool() { return m_graphicsCommandPool; }
    static VkCommandPool GetTransferCommandPool() { return m_transferCommandPool; }
    static VkFormat GetSwapchainImageFormat() { return m_swapchainImageFormat; }
    static VkFormat GetDepthFormat() { return m_depthFormat; }
    static VkFormat GetStencilFormat() { return m_stencilFormat; }
    static VkExtent2D GetSwapchainExtent() { return m_swapchainExtent; }

    static VkDescriptorSetLayout GetGlobalDescSetLayout() { return m_globalDescSetLayout; }
    static VkDescriptorSet GetGlobalDescSet() { return m_globalDescSet; }
    static VkPushConstantRange GetGlobalPushConstantRange() { return m_globalPushConstantRange; }

    static VkSampler GetTextureSampler() { return m_textureSampler; }

    static VmaAllocator GetVmaImageAllocator() { return m_vmaImageAllocator; }
    static VmaAllocator GetVmaBufferAllocator() { return m_vmaBufferAllocator; }

    static VkViewport GetViewport(uint32_t width, uint32_t height)
    {
        VkViewport viewport = {};
        viewport.width      = static_cast<float>(width);
        viewport.height     = -static_cast<float>(height);
        viewport.x          = 0.0f;
        viewport.y          = static_cast<float>(height);
        viewport.minDepth   = 0.0f;
        viewport.maxDepth   = 1.0f;
        return viewport;
    }


    static void Cleanup()
    {
        vmaDestroyAllocator(m_vmaImageAllocator);
        vmaDestroyAllocator(m_vmaBufferAllocator);

        vkDestroyCommandPool(m_device, m_graphicsCommandPool, nullptr);
        vkDestroyCommandPool(m_device, m_transferCommandPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_globalDescSetLayout, nullptr);

        vkDestroyDevice(m_device, nullptr);

#ifdef VDEBUG
        DestroyDebugUtilsMessengerEXT(m_instance, m_messenger, nullptr);
#endif

        vkDestroyInstance(m_instance, nullptr);
    }

#ifdef VDEBUG
    inline static PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT = nullptr;
    inline static PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT = nullptr;
    inline static PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT     = nullptr;
#endif

private:
    friend class Renderer;

    static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if(func != nullptr)
        {
            func(instance, debugMessenger, pAllocator);
        }
    }

    inline static VkDevice m_device                          = VK_NULL_HANDLE;
    inline static VkPhysicalDevice m_gpu                     = VK_NULL_HANDLE;
    inline static VkInstance m_instance                      = VK_NULL_HANDLE;
    inline static VkPhysicalDeviceProperties m_gpuProperties = {};

    inline static Queue m_graphicsQueue = {};
    inline static Queue m_transferQueue = {};
    inline static Queue m_computeQueue  = {};

    inline static VkCommandPool m_graphicsCommandPool = VK_NULL_HANDLE;
    inline static VkCommandPool m_transferCommandPool = VK_NULL_HANDLE;

    inline static VkDebugUtilsMessengerEXT m_messenger = VK_NULL_HANDLE;

    inline static VkFormat m_swapchainImageFormat = VK_FORMAT_UNDEFINED;
    inline static VkFormat m_depthFormat          = VK_FORMAT_D32_SFLOAT;
    inline static VkFormat m_stencilFormat        = VK_FORMAT_UNDEFINED;
    inline static VkExtent2D m_swapchainExtent    = {};

    inline static VkDescriptorSetLayout m_globalDescSetLayout   = VK_NULL_HANDLE;
    inline static VkDescriptorSet m_globalDescSet               = VK_NULL_HANDLE;
    inline static VkPushConstantRange m_globalPushConstantRange = {};

    inline static VkSampler m_textureSampler = VK_NULL_HANDLE;

    // The reason why we need 2 allocators is that with the buffer device address feature enabled on a VMA allocator
    // renderdoc crashes with a VK_DEVICE_LOST error when trying to capture a frame.
    // I managed to narrow it down to the fact that the crash happens when images are also created with the VMA allocator that has the feature enabled
    // This is a workaround for that.
    // So we use one allocator for images (without the buffer device address feature) and a separate one for buffers (with the buffer device address feature)
    inline static VmaAllocator m_vmaImageAllocator  = VK_NULL_HANDLE;
    inline static VmaAllocator m_vmaBufferAllocator = VK_NULL_HANDLE;
};


#ifdef VDEBUG
// NOLINTBEGIN (cppcoreguidelines-pro-type-cstyle-cast)
#define VK_SET_DEBUG_NAME(obj, objType, name)                                                        \
    {                                                                                                \
        if(VulkanContext::vkSetDebugUtilsObjectNameEXT == nullptr)                                   \
        {                                                                                            \
            VulkanContext::vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)         \
                vkGetInstanceProcAddr(VulkanContext::GetInstance(), "vkSetDebugUtilsObjectNameEXT"); \
        }                                                                                            \
        VkDebugUtilsObjectNameInfoEXT nameInfo = {};                                                 \
        nameInfo.sType                         = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT; \
        nameInfo.objectType                    = objType;                                            \
        nameInfo.objectHandle                  = (uint64_t)(obj);                                    \
        nameInfo.pObjectName                   = name;                                               \
        VulkanContext::vkSetDebugUtilsObjectNameEXT(VulkanContext::GetDevice(), &nameInfo);          \
    }

#define VK_START_DEBUG_LABEL(cmdBuffer, name)                                                        \
    {                                                                                                \
        if(VulkanContext::vkCmdBeginDebugUtilsLabelEXT == nullptr)                                   \
        {                                                                                            \
            VulkanContext::vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)         \
                vkGetInstanceProcAddr(VulkanContext::GetInstance(), "vkCmdBeginDebugUtilsLabelEXT"); \
        }                                                                                            \
        VkDebugUtilsLabelEXT label = {};                                                             \
        label.sType                = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;                        \
        label.pLabelName           = name;                                                           \
        label.color[0]             = 0.0f;                                                           \
        label.color[1]             = 1.0f;                                                           \
        label.color[2]             = 0.0f;                                                           \
        label.color[3]             = 1.0f;                                                           \
        VulkanContext::vkCmdBeginDebugUtilsLabelEXT(cmdBuffer.GetCommandBuffer(), &label);           \
    }

#define VK_END_DEBUG_LABEL(cmdBuffer)                                                              \
    {                                                                                              \
        if(VulkanContext::vkCmdEndDebugUtilsLabelEXT == nullptr)                                   \
        {                                                                                          \
            VulkanContext::vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)           \
                vkGetInstanceProcAddr(VulkanContext::GetInstance(), "vkCmdEndDebugUtilsLabelEXT"); \
        }                                                                                          \
        VulkanContext::vkCmdEndDebugUtilsLabelEXT(cmdBuffer.GetCommandBuffer());                   \
    }

// NOLINTEND (cppcoreguidelines-pro-type-cstyle-cast)
#else
#define VK_SET_DEBUG_NAME(obj, objType, name)
#define VK_START_DEBUG_LABEL(cmdBuffer, name)
#define VK_END_DEBUG_LABEL(cmdBuffer)
#endif
