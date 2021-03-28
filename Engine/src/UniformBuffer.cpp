#include "UniformBuffer.hpp"


UniformBuffer::UniformBuffer(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size, void* data) :
Buffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
{
    if(data != nullptr)
        Fill(data, size);
}

UniformBuffer::~UniformBuffer()
{
}

void UniformBuffer::Update(void* newData)
{
    Fill(newData, m_size);
}

VkDescriptorSetLayoutBinding UniformBuffer::GetDescriptorSetLayoutBinding(uint32_t binding, uint32_t count, VkShaderStageFlags stage)
{
    VkDescriptorSetLayoutBinding layoutBinding  = {};
    layoutBinding.binding                       = binding;
    layoutBinding.descriptorType                = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount               = count;
    layoutBinding.stageFlags                    = stage;
    layoutBinding.pImmutableSamplers            = nullptr; // this is for texture samplers

    return layoutBinding;
}

WriteDescriptorSet UniformBuffer::GetWriteDescriptorSet(uint32_t binding, VkDescriptorSet descriptorSet) const
{
    VkDescriptorBufferInfo bufferInfo   = {};
    bufferInfo.buffer                   = m_buffer;
    bufferInfo.offset                   = 0;
    bufferInfo.range                    = m_size;

    VkWriteDescriptorSet descriptorWrite    = {};
    descriptorWrite.sType                   = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet                  = descriptorSet;
    descriptorWrite.dstBinding              = binding;
    descriptorWrite.dstArrayElement         = 0;
    descriptorWrite.descriptorCount         = 1;
    descriptorWrite.descriptorType          = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    return WriteDescriptorSet(descriptorWrite, bufferInfo);
}
