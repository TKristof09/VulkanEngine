#include "Buffer.hpp"
#include "vma/vk_mem_alloc.h"
#include <iostream>
#include <algorithm>
#include <vadefs.h>

Buffer::Buffer() : m_size(0) {}

Buffer::Buffer(VkDeviceSize size, VkBufferUsageFlags usage, bool mappable)
{
    Allocate(size, usage, mappable);
}

Buffer::~Buffer()
{
    Free();
}

void Buffer::Free()
{
    if(m_buffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(VulkanContext::GetVmaAllocator(), m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
    }
}

void Buffer::Allocate(VkDeviceSize size, VkBufferUsageFlags usage, bool mappable)
{
    m_size = size;

    m_type = Buffer::Type::TRANSFER;
    if(usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        m_type = Buffer::Type::VERTEX;
    if(usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
        m_type = Buffer::Type::INDEX;
    if(usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        m_type = Buffer::Type::UNIFORM;

    VkBufferCreateInfo createInfo = {};
    createInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size               = size;
    createInfo.usage              = usage;
    createInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;  // TODO: apparently for buffers using concurrent is the same performance as exclusive, but it makes handling multiple queues easier

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage                   = VMA_MEMORY_USAGE_AUTO;
    if(mappable)
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;  // TODO: how to chose between sequential and random access

    VK_CHECK(vmaCreateBuffer(VulkanContext::GetVmaAllocator(), &createInfo, &allocInfo, &m_buffer, &m_allocation, nullptr), "Failed to create buffer");
}

void Buffer::Copy(Buffer* dst, VkDeviceSize size)
{
    CommandBuffer commandBuffer;
    commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset    = 0;
    copyRegion.dstOffset    = 0;
    copyRegion.size         = size;
    vkCmdCopyBuffer(commandBuffer.GetCommandBuffer(), m_buffer, dst->GetVkBuffer(), 1, &copyRegion);

    commandBuffer.SubmitIdle(VulkanContext::GetGraphicsQueue());  // TODO better to submit with a fence instead of waiting until idle
}

void Buffer::CopyToImage(VkImage image, uint32_t width, uint32_t height)
{
    CommandBuffer commandBuffer;
    commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkBufferImageCopy region = {};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer.GetCommandBuffer(), m_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);


    commandBuffer.SubmitIdle(VulkanContext::GetGraphicsQueue());
}

void Buffer::Fill(void* data, uint64_t size, uint64_t offset)
{
    void* memory = nullptr;
    VK_CHECK(vmaMapMemory(VulkanContext::GetVmaAllocator(), m_allocation, &memory), "Failed to map memory");

    memcpy((void*)((uintptr_t)memory + offset), data, (size_t)size);

    /* On desktop hardware host visible memory is also always host coherent
    if(!m_isHostCoherent)
        VK_CHECK(vmaFlushAllocation(VulkanContext::GetVmaAllocator(), m_allocation, offset, size), "Failed to flush memory");
*/
    vmaUnmapMemory(VulkanContext::GetVmaAllocator(), m_allocation);
}

void Buffer::Fill(std::vector<void*> datas, uint64_t size, std::vector<uint64_t> offsets)
{
    void* memory = nullptr;
    VK_CHECK(vmaMapMemory(VulkanContext::GetVmaAllocator(), m_allocation, &memory), "Failed to map memory");

    uint32_t i = 0;
    for(auto offset : offsets)
    {
        memcpy((void*)((uintptr_t)memory + offset), datas[i], (size_t)size);

        i++;
    }

    /* On desktop hardware host visible memory is also always host coherent
    if(!m_isHostCoherent)
    {
        std::vector<VkMappedMemoryRange> ranges(offsets.size());
        uint32_t i = 0;
        for(auto offset : offsets)
        {
            diff                 = offset % nonCoherentAtomSize;
            correctOffset        = offset - diff;
            uint64_t correctSize = (size + diff) + nonCoherentAtomSize - ((size + diff) % nonCoherentAtomSize);


            ranges[i].sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            ranges[i].memory = m_memory;
            ranges[i].size   = correctSize;
            ranges[i].offset = correctOffset;

            i++;
        }

        vkFlushMappedMemoryRanges(VulkanContext::GetDevice(), ranges.size(), ranges.data());
    }
    */
    vmaUnmapMemory(VulkanContext::GetVmaAllocator(), m_allocation);
}

void Buffer::Bind(const CommandBuffer& commandBuffer)
{
    switch(m_type)
    {
    case Type::VERTEX:
        {
            VkBuffer buffers[]     = {m_buffer};
            VkDeviceSize offsets[] = {0};
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
