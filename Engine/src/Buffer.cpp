#include "Buffer.hpp"
#include <iostream>
#include <algorithm>

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

Buffer::Buffer():
	m_size(VK_NULL_HANDLE) {}

Buffer::Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
	Allocate(size, usage, properties);
}

Buffer::~Buffer()
{
	if(m_buffer != VK_NULL_HANDLE)
		Free();
}

void Buffer::Allocate(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
	m_size = size;

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

	VK_CHECK(vkCreateBuffer(VulkanContext::GetDevice(), &createInfo, nullptr, &m_buffer), "Failed to create buffer");

	// find memory for buffer
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(VulkanContext::GetDevice(), m_buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo      = {};
	allocInfo.sType                     = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize            = memRequirements.size;
	allocInfo.memoryTypeIndex           = FindMemoryType(VulkanContext::GetPhysicalDevice(), memRequirements.memoryTypeBits, properties);

	VK_CHECK(vkAllocateMemory(VulkanContext::GetDevice(), &allocInfo, nullptr, &m_memory), "Failed to allocate buffer memory");

	VK_CHECK(vkBindBufferMemory(VulkanContext::GetDevice(), m_buffer, m_memory, 0), "Failed to bind buffer memory");
	
}

void Buffer::Free()
{
	vkDestroyBuffer(VulkanContext::GetDevice(), m_buffer, nullptr);
	vkFreeMemory(VulkanContext::GetDevice(), m_memory, nullptr);
	m_buffer = VK_NULL_HANDLE;
}

void Buffer::Copy(Buffer* dst, VkDeviceSize size)
{
	CommandBuffer commandBuffer;
	commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VkBufferCopy copyRegion                 = {};
	copyRegion.srcOffset                    = 0;
	copyRegion.dstOffset                    = 0;
	copyRegion.size                         = size;
	vkCmdCopyBuffer(commandBuffer.GetCommandBuffer(), m_buffer, dst->GetVkBuffer(), 1, &copyRegion);

	commandBuffer.SubmitIdle(VulkanContext::GetGraphicsQueue()); // TODO better to submit with a fence instead of waiting until idle
}

void Buffer::CopyToImage(VkImage image, uint32_t width, uint32_t height)
{
	CommandBuffer commandBuffer;
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


	commandBuffer.SubmitIdle(VulkanContext::GetGraphicsQueue());
}

void Buffer::Fill(void* data, uint64_t size, uint64_t offset, bool manualFlush)
{
	void* memory;

	// we need to make sure that the start and end of the memory are multiples of nonCoherentAtomSize
	// see https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkMapMemory.html
	uint64_t correctOffset = offset;
	uint64_t correctSize = size;
	VkDeviceSize nonCoherentAtomSize = VulkanContext::GetPhysicalDeviceProperties().limits.nonCoherentAtomSize;

	uint32_t diff = offset % nonCoherentAtomSize;

	correctOffset = offset - diff;
	correctSize = (size + diff) + nonCoherentAtomSize - ((size + diff) % nonCoherentAtomSize);
	if(correctOffset + correctSize > m_size)
	{
		correctSize = VK_WHOLE_SIZE;
	}



	VK_CHECK(vkMapMemory(VulkanContext::GetDevice(), m_memory, correctOffset, correctSize, 0, &memory), "Failed to map memory for vertex buffer");
	memcpy((void*)((uintptr_t)memory + diff),data, (size_t)size);

	if(manualFlush)
	{
		VkMappedMemoryRange range = {};
		range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory	= m_memory;
		range.size		= correctSize;
		range.offset	= correctOffset;

		vkFlushMappedMemoryRanges(VulkanContext::GetDevice(), 1, &range);
	}
	vkUnmapMemory(VulkanContext::GetDevice(), m_memory);

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
