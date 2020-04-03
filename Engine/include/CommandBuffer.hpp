#pragma once
#include <vulkan/vulkan.h>

class CommandBuffer
{

public:
    CommandBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY );
    ~CommandBuffer();
	void Begin(VkCommandBufferUsageFlags usage);
	void Begin(VkCommandBufferUsageFlags usage, VkCommandBufferInheritanceInfo inheritanceInfo);
    void End();

    void SubmitIdle(VkQueue queue);
    void Submit(VkQueue queue, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStage, VkSemaphore signalSemaphore, VkFence fence);

    const VkCommandBuffer& GetCommandBuffer() const
    {
        return m_commandBuffer;
    }
private:
    bool                m_running;
    VkCommandBuffer     m_commandBuffer;
    VkCommandPool       m_commandPool;
    VkDevice            m_device;

};
