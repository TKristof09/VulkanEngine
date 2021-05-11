#include "Memory/BufferAllocator.hpp"


BufferAllocator::BufferAllocator(uint32_t objSize, uint32_t objPerChunk, VkBufferUsageFlags flags)
:m_objSize(objSize), m_objPerChunk(objPerChunk), m_flags(flags)
{
	VkPhysicalDeviceProperties prop;
	vkGetPhysicalDeviceProperties(VulkanContext::GetPhysicalDevice(), &prop);
	// alignment code from Sascha Willems
	if(flags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
	{
		size_t minUboAlignment = prop.limits.minUniformBufferOffsetAlignment;
		m_alignedSize = objSize;
		if (minUboAlignment > 0)
		{
			m_alignedSize = (m_alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
		}
	}
	else if(flags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
	{
		size_t minUboAlignment = prop.limits.minStorageBufferOffsetAlignment;
		m_alignedSize = objSize;
		if (minUboAlignment > 0)
		{
			m_alignedSize = (m_alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
		}
	}

	m_buffers.push_back(Buffer(m_objPerChunk * m_alignedSize, flags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
	for (int i = 0; i < m_objPerChunk; i++)
	{
		m_freeSlots.push(i);
	}


}
BufferAllocator::~BufferAllocator()
{
	for(Buffer& buffer : m_buffers)
	{
		buffer.Free();
	}
}

uint32_t BufferAllocator::Allocate()
{
	if(m_freeSlots.empty())
	{
		uint32_t numBuffers = m_buffers.size();
		m_buffers.push_back(Buffer(m_objPerChunk * m_alignedSize, m_flags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
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
uint32_t BufferAllocator::GetBufferIndexAndOffset(uint32_t slot, uint32_t& outOffset)
{
	uint32_t bufferNumber = slot / m_objPerChunk;
	outOffset = (slot - bufferNumber * m_objPerChunk) * m_alignedSize;

	return bufferNumber;
}
void BufferAllocator::UpdateBuffer(uint32_t slot, void* data)
{
	uint32_t bufferNumber = slot / m_objPerChunk;
	uint32_t offset = (slot - bufferNumber) * m_alignedSize;

	m_buffers[bufferNumber].Fill(data, m_objSize, offset, true);
}
void BufferAllocator::UpdateBuffer(std::vector<std::pair<uint32_t, void*>> slotsAndDatas)
{
	if(slotsAndDatas.empty())
		return;
	
	std::sort(slotsAndDatas.begin(), slotsAndDatas.end(),
		[](const auto& lhs, const auto& rhs)
				{
					return lhs.first < rhs.first;
				});
	
	std::vector<uint64_t> offsets;
	uint32_t lastBufferNumber = 0;
	std::vector<void*> datas;
	
	for(auto& [slot, data] : slotsAndDatas)
	{
		uint32_t bufferNumber = slot / m_objPerChunk;
		uint64_t offset = (slot - bufferNumber) * m_alignedSize;

		if(bufferNumber != lastBufferNumber)
		{
			
			m_buffers[lastBufferNumber].Fill(datas, m_objSize, offsets, true);
			offsets.clear();
			datas.clear();
		}
		datas.push_back(data);
		offsets.push_back(offset);
		lastBufferNumber = bufferNumber;
	}

	// update the last buffer
	m_buffers[lastBufferNumber].Fill(datas, m_objSize, offsets, true);
	
}
void BufferAllocator::Free(uint32_t slot)
{
	m_freeSlots.push(slot);
}

