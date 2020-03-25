#pragma once
#include <vulkan/vulkan.h>
#include "Buffer.hpp"
#include "Descriptor.h"

class UniformBuffer : public Buffer, public Descriptor{

public:
    UniformBuffer(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size, void* data=nullptr);
    ~UniformBuffer();

    void Update(void* newData);
    static VkDescriptorSetLayoutBinding GetDescriptorSetLayoutBinding(uint32_t binding, uint32_t count, VkShaderStageFlags stage);
    WriteDescriptorSet GetWriteDescriptorSet(uint32_t binding, VkDescriptorSet descriptorSet) const override;
};
