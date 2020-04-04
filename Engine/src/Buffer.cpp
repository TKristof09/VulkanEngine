#include "Buffer.hpp"
#include <iostream>
#include <cstring>

uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if(typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

Buffer::Buffer(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties):
m_device(device),
m_size(size)
{
	
    m_type = Buffer::Type::TRANSFER;
    if(usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        m_type = Buffer::Type::VERTEX;
    if(usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
        m_type = Buffer::Type::INDEX;
    if(usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        m_type = Buffer::Type::UNIFORM;

    VkBufferCreateInfo createInfo       = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size                     = size;
    createInfo.usage                    = usage;
    createInfo.sharingMode              = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(m_device, &createInfo, nullptr, &m_buffer), "Failed to create buffer");

    // find memory for buffer
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, m_buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo      = {};
    allocInfo.sType                     = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize            = memRequirements.size;
    allocInfo.memoryTypeIndex           = FindMemoryType(gpu, memRequirements.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(m_device, &allocInfo, nullptr, &m_memory), "Failed to allocate buffer memory");

    VK_CHECK(vkBindBufferMemory(m_device, m_buffer, m_memory, 0), "Failed to bind buffer memory");
}

Buffer::~Buffer()
{
    vkDestroyBuffer(m_device, m_buffer, nullptr);
    vkFreeMemory(m_device, m_memory, nullptr);
}

void Buffer::Copy(Buffer* dst, VkDeviceSize size, VkQueue queue, VkCommandPool commandPool)
{
    VkCommandBufferAllocateInfo allocInfo   = {};
    allocInfo.sType                         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level                         = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool                   = commandPool;
    allocInfo.commandBufferCount            = 1;

    CommandBuffer commandBuffer(m_device, commandPool);
    commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkBufferCopy copyRegion                 = {};
    copyRegion.srcOffset                    = 0;
    copyRegion.dstOffset                    = 0;
    copyRegion.size                         = size;
    vkCmdCopyBuffer(commandBuffer.GetCommandBuffer(), m_buffer, dst->GetVkBuffer(), 1, &copyRegion);

    commandBuffer.SubmitIdle(queue); // TODO better to submit with a fence instead of waiting until idle
}

void Buffer::CopyToImage(VkImage image, uint32_t width, uint32_t height, VkCommandPool commandPool, VkQueue queue)
{
    CommandBuffer commandBuffer(m_device, commandPool);
    commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkBufferImageCopy region    = {};
    region.bufferOffset         = 0;
    region.bufferRowLength      = 0;
    region.bufferImageHeight    = 0;

    region.imageSubresource.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel        = 0;
    region.imageSubresource.baseArrayLayer  = 0;
    region.imageSubresource.layerCount      = 1;

    region.imageOffset          = {0, 0, 0};
    region.imageExtent          = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer.GetCommandBuffer(), m_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);


    commandBuffer.SubmitIdle(queue);
}

void Buffer::Fill(void* data, VkDeviceSize size)
{
    void* memory;
    VK_CHECK(vkMapMemory(m_device, m_memory, 0, size, 0, &memory), "Failed to map memory for vertex buffer");
    memcpy(memory, data, (size_t)size);
    vkUnmapMemory(m_device, m_memory);

}

void Buffer::Bind(const CommandBuffer& commandBuffer)
{
    switch(m_type)
    {
        case Type::VERTEX:
        {
            VkBuffer buffers[]                      = {m_buffer};
            VkDeviceSize offsets[]                  = {0};
            vkCmdBindVertexBuffers(commandBuffer.GetCommandBuffer(), 0, 1, buffers, offsets);
            break;
        }
        case Type::INDEX:
        {
            vkCmdBindIndexBuffer(commandBuffer.GetCommandBuffer(), m_buffer, 0, VK_INDEX_TYPE_UINT32);
            break;
        }
		default:
			std::cout << "Bufffer type not handled" << std::endl;
			break;
    }
}
