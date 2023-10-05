#pragma once
#include "Rendering/Buffer.hpp"
#include "Rendering/VulkanContext.hpp"
#include <queue>
class BufferAllocator
{
public:
    BufferAllocator() : m_objSize(0), m_objPerChunk(0), m_alignedSize(0) {}
    BufferAllocator(uint32_t objSize, uint32_t objPerChunk, VkBufferUsageFlags flags);
    ~BufferAllocator();

    uint32_t Allocate();
    uint32_t GetBufferIndexAndOffset(uint32_t slot, uint32_t& outOffset);
    void UpdateBuffer(uint32_t slot, void* data);
    void UpdateBuffer(std::vector<std::pair<uint32_t, void*>> slotsAndDatas);

    void Free(uint32_t slot);
    VkBuffer GetBuffer(uint32_t slot) { return m_buffers[slot / m_objPerChunk].GetVkBuffer(); }
    VkBuffer GetBufferByIndex(uint32_t index) { return m_buffers[index].GetVkBuffer(); }
    size_t GetSize() const { return m_alignedSize * m_objPerChunk; }
    size_t GetObjSize() const { return m_alignedSize; }

    Buffer* GetBufferPointer(uint32_t slot) { return &m_buffers[slot / m_objPerChunk]; }

private:
    uint32_t m_objSize;
    uint32_t m_objPerChunk;
    uint32_t m_alignedSize;
    VkBufferUsageFlags m_flags;

    std::queue<uint32_t> m_freeSlots;
    std::vector<Buffer> m_buffers;
};
