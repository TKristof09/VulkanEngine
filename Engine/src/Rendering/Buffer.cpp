#include "Buffer.hpp"
#include <iostream>
#include <algorithm>
#include "vulkan/vk_enum_string_helper.h"

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
        vmaDestroyBuffer(VulkanContext::GetVmaBufferAllocator(), m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
    }
}
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

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage                   = VMA_MEMORY_USAGE_AUTO;
    if(mappable)
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;  // TODO: how to chose between sequential and random access
    VmaAllocationInfo allocInfo;
    VK_CHECK(vmaCreateBuffer(VulkanContext::GetVmaBufferAllocator(), &createInfo, &allocCreateInfo, &m_buffer, &m_allocation, &allocInfo), "Failed to create buffer");
    m_mappedMemory = allocInfo.pMappedData;
    VkMemoryPropertyFlags memPropFlags;
    vmaGetAllocationMemoryProperties(VulkanContext::GetVmaBufferAllocator(), m_allocation, &memPropFlags);
    LOG_INFO("Memory type: {}", string_VkMemoryPropertyFlags(memPropFlags));
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
    if(m_mappedMemory)
    {
        memcpy((void*)((uintptr_t)m_mappedMemory + offset), data, (size_t)size);
        return;
    }

    void* memory = nullptr;
    VK_CHECK(vmaMapMemory(VulkanContext::GetVmaBufferAllocator(), m_allocation, &memory), "Failed to map memory");

    memcpy((void*)((uintptr_t)memory + offset), data, (size_t)size);

    /* On desktop hardware host visible memory is also always host coherent
    if(!m_isHostCoherent)
        VK_CHECK(vmaFlushAllocation(VulkanContext::GetVmaAllocator(), m_allocation, offset, size), "Failed to flush memory");
*/
    vmaUnmapMemory(VulkanContext::GetVmaBufferAllocator(), m_allocation);
}

void Buffer::Fill(const std::vector<void*>& datas, const std::vector<uint64_t>& sizes, const std::vector<uint64_t>& offsets)
{
    void* memory = nullptr;
    VK_CHECK(vmaMapMemory(VulkanContext::GetVmaBufferAllocator(), m_allocation, &memory), "Failed to map memory");

    uint32_t i = 0;
    for(auto offset : offsets)
    {
        memcpy((void*)((uintptr_t)memory + offset), datas[i], (size_t)sizes[i]);

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
    vmaUnmapMemory(VulkanContext::GetVmaBufferAllocator(), m_allocation);
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
        LOG_ERROR("Bufffer type not handled");
        break;
    }
}

DynamicBufferAllocator::DynamicBufferAllocator(DynamicBufferAllocator&& other) noexcept
    : m_currentSize(other.m_currentSize),
      m_elementSize(other.m_elementSize),
      m_stagingBufferSize(other.m_stagingBufferSize),
      m_usage(other.m_usage),
      m_mappable(other.m_mappable),
      m_buffer(std::move(other.m_buffer)),
      m_stagingBuffer(std::move(other.m_stagingBuffer)),
      m_tempBuffer(std::move(other.m_tempBuffer)),
      m_needDelete(other.m_needDelete),
      m_block(other.m_block),
      m_tempBlock(other.m_tempBlock),
      m_allocations(std::move(other.m_allocations)),
      m_fence(other.m_fence)
{
    other.m_fence = VK_NULL_HANDLE;
}
DynamicBufferAllocator& DynamicBufferAllocator::operator=(DynamicBufferAllocator&& other) noexcept
{
    if(this == &other)
        return *this;
    m_currentSize       = other.m_currentSize;
    m_elementSize       = other.m_elementSize;
    m_stagingBufferSize = other.m_stagingBufferSize;
    m_usage             = other.m_usage;
    m_mappable          = other.m_mappable;
    m_buffer            = std::move(other.m_buffer);
    m_stagingBuffer     = std::move(other.m_stagingBuffer);
    m_tempBuffer        = std::move(other.m_tempBuffer);
    m_needDelete        = other.m_needDelete;
    m_block             = other.m_block;
    m_tempBlock         = other.m_tempBlock;
    m_allocations       = std::move(other.m_allocations);
    m_fence             = other.m_fence;

    other.m_fence = VK_NULL_HANDLE;
    return *this;
}

uint64_t DynamicBufferAllocator::Allocate(uint64_t numObjects, bool& didResize, void* pUserData)
{
    if(m_buffer.GetVkBuffer() == VK_NULL_HANDLE)
    {
        Initialize();
    }
    VmaVirtualAllocationCreateInfo allocInfo = {};
    VmaVirtualAllocation allocation          = nullptr;
    uint64_t offset                          = 0;
    VkResult res                             = VK_ERROR_OUT_OF_POOL_MEMORY;

    allocInfo.size = numObjects * m_elementSize;
    // allocInfo.alignment = m_elementSize;

    // find usable allocation slot
    res = vmaVirtualAllocate(m_block, &allocInfo, &allocation, &offset);
    // couldn't fine a usable allocation slot, resize buffer
    if(res != VK_SUCCESS)
    {
        Resize();
        VK_CHECK(vmaVirtualAllocate(m_block, &allocInfo, &allocation, &offset), "Failed to allocate virtual memory after new block creation");
        didResize    = true;
        m_needDelete = true;
    }

    if(pUserData)
        vmaSetVirtualAllocationUserData(m_block, allocation, pUserData);

    uint64_t slot       = offset / m_elementSize;
    m_allocations[slot] = allocation;
    return slot;
}

void DynamicBufferAllocator::UploadData(uint64_t slot, void* data)
{
    auto it = m_allocations.find(slot);
    if(it == m_allocations.end())
    {
        LOG_ERROR("Dynamic buffer: no allocation with this offset");
        return;
    }

    VmaVirtualAllocationInfo allocInfo = {};
    vmaGetVirtualAllocationInfo(m_block, it->second, &allocInfo);

    Buffer* dst = m_needDelete ? &m_tempBuffer : &m_buffer;
    if(m_mappable)
    {
        dst->Fill(data, allocInfo.size, allocInfo.offset);
    }
    else
    {
        m_stagingBuffer.Fill(data, allocInfo.size, 0);
        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset    = 0;
        copyRegion.dstOffset    = allocInfo.offset;
        copyRegion.size         = allocInfo.size;

        CommandBuffer cb;
        vkDeviceWaitIdle(VulkanContext::GetDevice());
        cb.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        vkCmdCopyBuffer(cb.GetCommandBuffer(), m_stagingBuffer.GetVkBuffer(), dst->GetVkBuffer(), 1, &copyRegion);

        cb.Submit(VulkanContext::GetGraphicsQueue(), VK_NULL_HANDLE, 0, VK_NULL_HANDLE, m_fence);  // TODO: use transfer queue
        vkWaitForFences(VulkanContext::GetDevice(), 1, &m_fence, VK_TRUE, UINT64_MAX);
    }
}

void DynamicBufferAllocator::UploadData(const std::vector<uint64_t>& slots, const std::vector<void*>& datas)
{
    /*
    std::vector<uint64_t> sizes;
    std::vector<uint64_t> offsets;

    for(uint64_t slot : slots)
    {
        auto it = m_allocations.find(slot);
        if(it == m_allocations.end())
        {
            LOG_ERROR("Dynamic buffer: no allocation with this offset");
            return;
        }

        VmaVirtualAllocationInfo allocInfo = {};
        vmaGetVirtualAllocationInfo(m_block, it->second, &allocInfo);
        sizes.push_back(allocInfo.size);
        offsets.push_back(allocInfo.offset);
    }

    Buffer* dst = m_needDelete ? &m_tempBuffer : &m_buffer;
    if(m_mappable)
    {
        dst->Fill(datas, sizes, offsets);
    }
    else
    {
        m_stagingBuffer.Fill(datas, );
        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset    = 0;
        copyRegion.dstOffset    = allocInfo.offset;
        copyRegion.size         = allocInfo.size;

        CommandBuffer cb;
        vkDeviceWaitIdle(VulkanContext::GetDevice());
        cb.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        vkCmdCopyBuffer(cb.GetCommandBuffer(), m_stagingBuffer.GetVkBuffer(), dst->GetVkBuffer(), 1, &copyRegion);

        cb.Submit(VulkanContext::GetGraphicsQueue(), VK_NULL_HANDLE, 0, VK_NULL_HANDLE, m_fence);  // TODO: use transfer queue
        vkWaitForFences(VulkanContext::GetDevice(), 1, &m_fence, VK_TRUE, UINT64_MAX);
    }
}
void DynamicBufferAllocator::Free(uint64_t slot)
{
    auto it = m_allocations.find(slot);
    if(it == m_allocations.end())
    {
        LOG_ERROR("Dynamic buffer: no allocation with this offset");
        return;
    }

    vmaVirtualFree(m_block, it->second);
    m_allocations.erase(it);
    */
}

void DynamicBufferAllocator::Resize()
{
    m_currentSize *= 2;
    m_tempBuffer.Allocate(m_currentSize, m_usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, m_mappable);

    VmaVirtualBlockCreateInfo blockCreateInfo = {};
    blockCreateInfo.size                      = m_currentSize;

    VmaVirtualBlock block = nullptr;
    VK_CHECK(vmaCreateVirtualBlock(&blockCreateInfo, &block), "Failed to create virtual block");

    std::vector<VkBufferCopy> copyRegions;
    copyRegions.reserve(m_allocations.size());

    // do it 1 by 1 instead of copying the whole buffer in order to reduce fragmentation in case the old buffer had things freed from it which left it fragmented
    for(auto [slot, allocation] : m_allocations)
    {
        VmaVirtualAllocationInfo allocInfo = {};
        vmaGetVirtualAllocationInfo(m_block, allocation, &allocInfo);

        VmaVirtualAllocationCreateInfo newAllocInfo = {};
        VmaVirtualAllocation newAllocation          = nullptr;

        uint64_t offset = 0;

        newAllocInfo.size      = allocInfo.size;
        newAllocInfo.alignment = m_elementSize;

        // find slot in new buffer
        VK_CHECK(vmaVirtualAllocate(block, &newAllocInfo, &newAllocation, &offset), "Failed to allocate virtual memory after new block creation");


        copyRegions.push_back({allocInfo.offset, offset, allocInfo.size});
    }
    m_block = block;
    CommandBuffer cb;
    cb.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    // don't need to sync because we only read from the old buffer
    vkCmdCopyBuffer(cb.GetCommandBuffer(), m_buffer.GetVkBuffer(), m_tempBuffer.GetVkBuffer(), copyRegions.size(), copyRegions.data());

    cb.Submit(VulkanContext::GetGraphicsQueue(), VK_NULL_HANDLE, 0, VK_NULL_HANDLE, m_fence);  // TODO: use transfer queue
    vkWaitForFences(VulkanContext::GetDevice(), 1, &m_fence, VK_TRUE, UINT64_MAX);
}

void DynamicBufferAllocator::DeleteOldIfNeeded()
{
    if(m_needDelete)
    {
        m_buffer.Free();
        m_buffer     = std::move(m_tempBuffer);
        m_needDelete = false;
    }
}
