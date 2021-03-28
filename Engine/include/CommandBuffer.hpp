#pragma once
#include "VulkanContext.hpp"

class CommandBuffer
{

public:
    CommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    void Free();
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
    void Allocate(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    bool                m_running;
    VkCommandBuffer     m_commandBuffer;

};
