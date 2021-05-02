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
private:
	friend class Renderer;

	static VkDevice m_device;
	static VkPhysicalDevice m_gpu;
	static VkInstance m_instance;
	static VkPhysicalDeviceProperties m_gpuProperties;

	static VkQueue m_graphicsQueue;
	static VkQueue m_transferQueue;
	static VkQueue m_computeQueue;

	static VkCommandPool m_commandPool;
};
