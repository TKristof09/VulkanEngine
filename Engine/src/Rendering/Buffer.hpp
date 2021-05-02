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
	~Buffer();
	Buffer(const Buffer& other) = delete;

	Buffer(Buffer&& other) noexcept
		: m_type(other.m_type),
		  m_buffer(other.m_buffer),
		  m_memory(other.m_memory),
		  m_size(other.m_size),
		  m_nonCoherentAtomeSize(other.m_nonCoherentAtomeSize)
	{
		other.m_buffer = VK_NULL_HANDLE;
	}

	Buffer& operator=(const Buffer& other) = delete;

	Buffer& operator=(Buffer&& other) noexcept
	{
		if (this == &other)
			return *this;
		m_type                 = other.m_type;
		m_buffer               = other.m_buffer;
		other.m_buffer = VK_NULL_HANDLE;
		
		m_memory               = other.m_memory;
		m_size                 = other.m_size;
		m_nonCoherentAtomeSize = other.m_nonCoherentAtomeSize;
		return *this;
	}

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
