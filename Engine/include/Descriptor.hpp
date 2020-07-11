
#pragma once
#include <vulkan/vulkan.h>
#include <EASTL/unique_ptr.h>

class WriteDescriptorSet
{
public:
	WriteDescriptorSet(const VkWriteDescriptorSet& writeDescriptorSet, const VkDescriptorImageInfo& imageInfo) :
		m_writeDescriptorSet(writeDescriptorSet),
		m_imageInfo(eastl::make_unique<VkDescriptorImageInfo>(imageInfo)),
		m_bufferInfo(nullptr)
	{
		m_writeDescriptorSet.pImageInfo = m_imageInfo.get();
	}

	WriteDescriptorSet(const VkWriteDescriptorSet& writeDescriptorSet, const VkDescriptorBufferInfo& bufferInfo) :
		m_writeDescriptorSet(writeDescriptorSet),
		m_imageInfo(nullptr),
		m_bufferInfo(eastl::make_unique<VkDescriptorBufferInfo>(bufferInfo))
	{
		m_writeDescriptorSet.pBufferInfo = m_bufferInfo.get();
	}

	const VkWriteDescriptorSet& GetWriteDescriptorSet() const { return m_writeDescriptorSet; }

private:
	VkWriteDescriptorSet m_writeDescriptorSet;
	eastl::unique_ptr<VkDescriptorImageInfo> m_imageInfo;
	eastl::unique_ptr<VkDescriptorBufferInfo> m_bufferInfo;
};

class Descriptor
{
public:
    virtual WriteDescriptorSet GetWriteDescriptorSet(uint32_t binding, VkDescriptorSet descriptorSet) const = 0;
    Descriptor() = default;
    virtual ~Descriptor() = default;
};
