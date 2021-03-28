#include "Memory/UniformBufferAllocator.hpp"


UniformBufferAllocator::UniformBufferAllocator(uint32_t objSize, uint32_t objPerChunk)
:m_objSize(objSize), m_objPerChunk(objPerChunk)
{
	VkPhysicalDeviceProperties prop;
	vkGetPhysicalDeviceProperties(VulkanContext::GetPhysicalDevice(), &prop);
	// alignment code from Sascha Willems
	size_t minUboAlignment = prop.limits.minUniformBufferOffsetAlignment;
	m_alignedSize = objSize;
	if (minUboAlignment > 0)
	{
		m_alignedSize = (m_alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}

	m_buffers.push_back(Buffer(m_objPerChunk * m_alignedSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
	for (int i = 0; i < m_objPerChunk; i++)
	{
		m_freeSlots.push(i);
	}


}
UniformBufferAllocator::~UniformBufferAllocator()
{
	for(Buffer& buffer : m_buffers)
	{
		buffer.Free();
	}
}

uint32_t UniformBufferAllocator::Allocate()
{
	if(m_freeSlots.empty())
	{
		uint32_t numBuffers = m_buffers.size();
		m_buffers.push_back(Buffer(m_objPerChunk * m_alignedSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
		uint32_t end = (numBuffers + 1) * m_objPerChunk;
		uint32_t start = numBuffers * m_objPerChunk + 1;
		for (int i = start; i < end; i++)
		{
			m_freeSlots.push(i);
		}




		return start;
	}
	else
	{
		uint32_t freeSlot = m_freeSlots.front();
		m_freeSlots.pop();
		return freeSlot;
	}
}
VkBuffer UniformBufferAllocator::GetBufferAndOffset(uint32_t slot, uint32_t& outOffset)
{
	uint32_t bufferNumber = slot / m_objPerChunk;
	outOffset = (slot - bufferNumber * m_objPerChunk) * m_alignedSize;

	return m_buffers[bufferNumber].GetVkBuffer();
}
void UniformBufferAllocator::UpdateBuffer(uint32_t slot, void* data)
{
	uint32_t bufferNumber = slot / m_objPerChunk;
	uint32_t offset = (slot - bufferNumber) * m_alignedSize;

	m_buffers[bufferNumber].Fill(data, m_objSize, offset, true);
}
void UniformBufferAllocator::Free(uint32_t slot)
{
	m_freeSlots.push(slot);
}

