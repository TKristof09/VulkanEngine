#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "CommandBuffer.hpp"
uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);



// TODO implement https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
class Buffer
{
public:
	enum class Type
	{
		VERTEX,
		INDEX,
		UNIFORM,

		TRANSFER
	};
	Buffer();
	Buffer(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

	void Allocate(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
	void Free();
	void Copy(Buffer* dst, VkDeviceSize size, VkQueue queue, VkCommandPool commandPool);
	void CopyToImage(VkImage image, uint32_t width, uint32_t height, VkCommandPool commandPool, VkQueue queue);
	void Fill(void* data, VkDeviceSize size, uint32_t offset = 0, bool manualFlush = false); // TODO change the manual flush to true by default i think
	void Bind(const CommandBuffer& commandBuffer);
	const VkBuffer& GetVkBuffer() const
	{
		return m_buffer;
	}
	const VkDeviceSize GetSize() const { return m_size; }

protected:
	Type								m_type;
	VkDevice							m_device;
	VkBuffer							m_buffer        = VK_NULL_HANDLE;
	VkDeviceMemory						m_memory        = VK_NULL_HANDLE;
	VkDeviceSize						m_size;
};
