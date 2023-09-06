#pragma once

#include "vulkan/vulkan_core.h"
#include <vulkan/vulkan.h>

struct VulkanContext
{
	static VkInstance GetInstance() { return m_instance; }
	static VkDevice GetDevice() { return m_device; }
	static VkPhysicalDevice GetPhysicalDevice() { return m_gpu; }
	static VkPhysicalDeviceProperties GetPhysicalDeviceProperties() { return m_gpuProperties; }
	static VkQueue GetGraphicsQueue() { return m_graphicsQueue; }
	static VkCommandPool GetCommandPool() { return m_commandPool; }
    static VkFormat GetSwapchainImageFormat() { return m_swapchainImageFormat; }
    static VkFormat GetDepthFormat() { return m_depthFormat; }
    static VkFormat GetStencilFormat() { return m_stencilFormat; }

	static void Cleanup()
	{
		vkDestroyDevice(m_device, nullptr);

#ifdef VDEBUG
		DestroyDebugUtilsMessengerEXT(m_instance, m_messenger, nullptr);
#endif

		vkDestroyInstance(m_instance, nullptr);
	}
private:
	friend class Renderer;

	static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			func(instance, debugMessenger, pAllocator);
		}
	}

	inline static VkDevice m_device = VK_NULL_HANDLE;
	inline static VkPhysicalDevice m_gpu = VK_NULL_HANDLE;
	inline static VkInstance m_instance = VK_NULL_HANDLE;
	inline static VkPhysicalDeviceProperties m_gpuProperties = {};

	inline static VkQueue m_graphicsQueue = VK_NULL_HANDLE;
	inline static VkQueue m_transferQueue = VK_NULL_HANDLE;
	inline static VkQueue m_computeQueue = VK_NULL_HANDLE;

	inline static VkCommandPool m_commandPool = VK_NULL_HANDLE;

	inline static VkDebugUtilsMessengerEXT  m_messenger = VK_NULL_HANDLE;

    inline static VkFormat m_swapchainImageFormat = VK_FORMAT_UNDEFINED;
    inline static VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
    inline static VkFormat m_stencilFormat = VK_FORMAT_UNDEFINED;
};
