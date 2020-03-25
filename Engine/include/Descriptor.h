
#pragma once
#include <vulkan/vulkan.h>
#include <memory>


class WriteDescriptorSet
{
public:
	WriteDescriptorSet(const VkWriteDescriptorSet& writeDescriptorSet, const VkDescriptorImageInfo& imageInfo) :
		m_writeDescriptorSet(writeDescriptorSet),
		m_imageInfo(std::make_unique<VkDescriptorImageInfo>(imageInfo)),
		m_bufferInfo(nullptr)
	{
		m_writeDescriptorSet.pImageInfo = m_imageInfo.get();
	}

	WriteDescriptorSet(const VkWriteDescriptorSet& writeDescriptorSet, const VkDescriptorBufferInfo& bufferInfo) :
		m_writeDescriptorSet(writeDescriptorSet),
		m_imageInfo(nullptr),
		m_bufferInfo(std::make_unique<VkDescriptorBufferInfo>(bufferInfo))
	{
		m_writeDescriptorSet.pBufferInfo = m_bufferInfo.get();
	}

	const VkWriteDescriptorSet& GetWriteDescriptorSet() const { return m_writeDescriptorSet; }

private:
	VkWriteDescriptorSet m_writeDescriptorSet;
	std::unique_ptr<VkDescriptorImageInfo> m_imageInfo;
	std::unique_ptr<VkDescriptorBufferInfo> m_bufferInfo;
};

class Descriptor
{
public:
    virtual WriteDescriptorSet GetWriteDescriptorSet(uint32_t binding, VkDescriptorSet descriptorSet) const = 0;
    Descriptor() = default;
    virtual ~Descriptor() = default;
};
