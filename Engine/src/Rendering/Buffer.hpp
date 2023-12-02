#pragma once
#include "VulkanContext.hpp"
#include "CommandBuffer.hpp"
#include <vma/vk_mem_alloc.h>


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
    Buffer(VkDeviceSize size, VkBufferUsageFlags usage, bool mappable = false);
    ~Buffer();
    Buffer(const Buffer& other) = delete;

    Buffer(Buffer&& other) noexcept
        : m_type(other.m_type),
          m_buffer(other.m_buffer),
          m_size(other.m_size),
          m_nonCoherentAtomeSize(other.m_nonCoherentAtomeSize),
          m_allocation(other.m_allocation)
    {
        other.m_buffer = VK_NULL_HANDLE;
    }

    Buffer& operator=(const Buffer& other) = delete;

    Buffer& operator=(Buffer&& other) noexcept
    {
        if(this == &other)
            return *this;
        m_type                 = other.m_type;
        m_buffer               = other.m_buffer;
        m_size                 = other.m_size;
        m_nonCoherentAtomeSize = other.m_nonCoherentAtomeSize;
        m_allocation           = other.m_allocation;

        other.m_buffer = VK_NULL_HANDLE;
        return *this;
    }

    void Allocate(VkDeviceSize size, VkBufferUsageFlags usage, bool mappable = false);
    void Free();
    void Copy(Buffer* dst, VkDeviceSize size);
    void CopyToImage(VkImage image, uint32_t width, uint32_t height);
    void Fill(const void* data, uint64_t size, uint64_t offset = 0);

    // offsets must be sorted in ascending order
    void Fill(const std::vector<const void*>& datas, const std::vector<uint64_t>& sizes, const std::vector<uint64_t>& offsets);
    void Bind(const CommandBuffer& commandBuffer);
    [[nodiscard]] const VkBuffer& GetVkBuffer() const { return m_buffer; }
    [[nodiscard]] VkDeviceSize GetSize() const { return m_size; }
    [[nodiscard]] uint64_t GetDeviceAddress() const
    {
        VkBufferDeviceAddressInfo info = {};
        info.sType                     = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer                    = m_buffer;

        return vkGetBufferDeviceAddress(VulkanContext::GetDevice(), &info);
    }

protected:
    Type m_type;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    uint64_t m_size;

    uint64_t m_nonCoherentAtomeSize;

    VmaAllocation m_allocation;
    void* m_mappedMemory = nullptr;
};


class DynamicBufferAllocator
{
public:
    using ResizeCallback = std::function<void(DynamicBufferAllocator*)>;

    // stagingBufferSize is ignored if mappable is true
    DynamicBufferAllocator(uint64_t startingSize, uint32_t elementSize, VkBufferUsageFlags usage, uint64_t stagingBufferSize, bool mappable = false)
        : m_currentSize(startingSize * elementSize),
          m_elementSize(elementSize),
          m_stagingBufferSize(stagingBufferSize * elementSize),
          m_mappable(mappable),
          m_usage(usage)
    {
    }
    DynamicBufferAllocator(uint64_t startingSize, uint32_t elementSize, VkBufferUsageFlags usage, uint64_t stagingBufferSize, ResizeCallback callback, bool mappable = false)
        : m_currentSize(startingSize * elementSize),
          m_elementSize(elementSize),
          m_stagingBufferSize(stagingBufferSize * elementSize),
          m_mappable(mappable),
          m_usage(usage),
          m_resizeCallback(callback)
    {
    }

    ~DynamicBufferAllocator()
    {
        if(m_fence != VK_NULL_HANDLE)
        {
            vkDestroyFence(VulkanContext::GetDevice(), m_fence, nullptr);
        }
    }

    // delete copy constructor and assignment
    DynamicBufferAllocator(const DynamicBufferAllocator&)            = delete;
    DynamicBufferAllocator& operator=(const DynamicBufferAllocator&) = delete;

    // move constructor and assignment definiton
    DynamicBufferAllocator(DynamicBufferAllocator&& other) noexcept;
    DynamicBufferAllocator& operator=(DynamicBufferAllocator&& other) noexcept;


    uint64_t Allocate(uint64_t numObjects, bool& didResize, void* pUserData = nullptr);
    uint64_t Allocate(uint64_t numObjects, void* pUserData = nullptr);
    void UploadData(uint64_t slot, const void* data);
    void UploadData(const std::vector<uint64_t>& slots, const std::vector<const void*>& datas);

    void Free(uint64_t slot);

    void DeleteOldIfNeeded();

    void Bind(CommandBuffer& cb) { m_buffer.Bind(cb); }

    [[nodiscard]] uint64_t GetDeviceAddress(uint64_t slot)
    {
        // TODO make initialisation not messy
        if(m_buffer.GetVkBuffer() == VK_NULL_HANDLE)
        {
            Initialize();
        }
        VkBufferDeviceAddressInfo info = {};
        info.sType                     = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer                    = m_buffer.GetVkBuffer();

        VkDeviceAddress baseAddress = vkGetBufferDeviceAddress(VulkanContext::GetDevice(), &info);
        return baseAddress + slot * m_elementSize;
    }

    [[nodiscard]] std::unordered_map<uint64_t, VmaVirtualAllocationInfo> GetAllocationInfos() const
    {
        std::unordered_map<uint64_t, VmaVirtualAllocationInfo> infos;
        for(const auto& [slot, allocation] : m_allocations)
        {
            vmaGetVirtualAllocationInfo(m_block, allocation, &infos[slot]);
        }
        return infos;
    }

private:
    void Resize();
    void Initialize()
    {
        if(!m_mappable)
        {
            m_stagingBuffer.Allocate(m_stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);
            m_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }
        m_buffer.Allocate(m_currentSize, m_usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, m_mappable);
        VmaVirtualBlockCreateInfo blockCreateInfo = {};
        blockCreateInfo.size                      = m_currentSize;

        VK_CHECK(vmaCreateVirtualBlock(&blockCreateInfo, &m_block), "Failed to create virtual block");

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VK_CHECK(vkCreateFence(VulkanContext::GetDevice(), &fenceCreateInfo, nullptr, &m_fence), "Failed to create fence");
    }

    uint64_t m_currentSize;        // in bytes
    uint32_t m_elementSize;        // in bytes
    uint64_t m_stagingBufferSize;  // in bytes
    VkBufferUsageFlags m_usage;
    bool m_mappable;
    ResizeCallback m_resizeCallback;

    Buffer m_buffer;
    Buffer m_stagingBuffer;
    Buffer m_tempBuffer;  // used when resizing in order to not destroy the old buffer while the gpu is working
    bool m_needDelete           = false;
    VmaVirtualBlock m_block     = nullptr;
    VmaVirtualBlock m_tempBlock = nullptr;
    std::unordered_map<uint64_t, VmaVirtualAllocation> m_allocations;

    VkFence m_fence{};
};
