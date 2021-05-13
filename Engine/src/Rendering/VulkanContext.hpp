#pragma once

#include <vulkan/vulkan.h>

struct VulkanContext
{
	static VkInstance GetInstance() { return m_instance; }
	static VkDevice GetDevice() { return m_device; }
	static VkPhysicalDevice GetPhysicalDevice() { return m_gpu; }
	static VkPhysicalDeviceProperties GetPhysicalDeviceProperties() { return m_gpuProperties; }
	static VkQueue GetGraphicsQueue() { return m_graphicsQueue; }
	static VkCommandPool GetCommandPool() { return m_commandPool; }

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
};
