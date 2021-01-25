#pragma once

#include <vulkan/vulkan.h>

struct VulkanContext
{
	static VkDevice GetDevice() { return m_device; }
	static VkPhysicalDevice GetPhysicalDevice() { return m_gpu; }
private:
	friend class RendererSystem;

	static VkDevice m_device;
	static VkPhysicalDevice m_gpu;

	static VkQueue m_graphicsQueue;
	static VkQueue m_transferQueue;
	static VkQueue m_computeQueue;

};
