#pragma once
#include <glm/glm.hpp>
#include "VulkanContext.hpp"
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
	Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

	void Allocate(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
	void Free();
	void Copy(Buffer* dst, VkDeviceSize size);
	void CopyToImage(VkImage image, uint32_t width, uint32_t height);
	void Fill(void* data, uint64_t size, uint64_t offset = 0, bool manualFlush = false); // TODO change the manual flush to true by default i think
	void Bind(const CommandBuffer& commandBuffer);
	const VkBuffer& GetVkBuffer() const
	{
		return m_buffer;
	}
	const VkDeviceSize GetSize() const { return m_size; }

protected:
	Type								m_type;
	VkBuffer							m_buffer        = VK_NULL_HANDLE;
	VkDeviceMemory						m_memory        = VK_NULL_HANDLE;
	uint64_t							m_size;

	uint64_t							m_nonCoherentAtomeSize;
};
