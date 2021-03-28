#pragma once
#include "Buffer.hpp"
#include "VulkanContext.hpp"
#include <queue>
class UniformBufferAllocator
{
public:
	UniformBufferAllocator():
		m_objSize(0), m_objPerChunk(0), m_alignedSize(0) {}
	UniformBufferAllocator(uint32_t objSize, uint32_t objPerChunk);
	~UniformBufferAllocator();

	uint32_t Allocate();
	VkBuffer GetBufferAndOffset(uint32_t slot, uint32_t& outOffset);
	void UpdateBuffer(uint32_t slot, void* data);
	void Free(uint32_t slot);
	VkBuffer GetBuffer(uint32_t slot) { return m_buffers[slot / m_objPerChunk].GetVkBuffer(); }
	size_t GetSize() const { return m_alignedSize * m_objPerChunk; }
	size_t GetObjSize() const { return m_alignedSize; }

private:


	uint32_t m_objSize;
	uint32_t m_objPerChunk;
	uint32_t m_alignedSize;

	std::queue<uint32_t> m_freeSlots;
	std::vector<Buffer> m_buffers;
};

