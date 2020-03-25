#pragma once
#include <glm/glm.hpp>
#include <array>
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
    Buffer(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    ~Buffer();
    void Copy(Buffer* dst, VkDeviceSize size, VkQueue queue, VkCommandPool commandPool);
    void CopyToImage(VkImage image, uint32_t width, uint32_t height, VkCommandPool commandPool, VkQueue queue);
    void Fill(void* data, VkDeviceSize size);
    void Bind(const CommandBuffer& commandBuffer);
    const VkBuffer& GetVkBuffer() const
    {
        return m_buffer;
    }

protected:
    Type								m_type;
	VkDevice							m_device;
    VkBuffer							m_buffer        = VK_NULL_HANDLE;
    VkDeviceMemory						m_memory        = VK_NULL_HANDLE;
    VkDeviceSize						m_size;
};
