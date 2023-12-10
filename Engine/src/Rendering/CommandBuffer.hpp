#pragma once
#include "VulkanContext.hpp"

class CommandBuffer
{
public:
    CommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    ~CommandBuffer();
    void Free();
    void Begin(VkCommandBufferUsageFlags usage);
    void Begin(VkCommandBufferUsageFlags usage, VkCommandBufferInheritanceInfo inheritanceInfo);
    void End();

    void SubmitIdle(VkQueue queue = VulkanContext::GetGraphicsQueue());
    void Submit(VkQueue queue, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStage, VkSemaphore signalSemaphore, VkFence fence);
    void Submit(VkQueue queue, const std::vector<VkSemaphore>& waitSemaphores, const std::vector<VkPipelineStageFlags>& waitStages, const std::vector<VkSemaphore>& signalSemaphores, VkFence fence);

    void Reset();

    void BindGlobalDescSet(VkPipelineBindPoint bindPoint, VkPipelineLayout layout);

    [[nodiscard]] const VkCommandBuffer& GetCommandBuffer() const
    {
        return m_commandBuffer;
    }

    CommandBuffer(const CommandBuffer& other) = delete;

    CommandBuffer(CommandBuffer&& other) noexcept
        : m_recording(other.m_recording),
          m_commandBuffer(other.m_commandBuffer)
    {
        other.m_commandBuffer = VK_NULL_HANDLE;
    }

    CommandBuffer& operator=(const CommandBuffer& other) = delete;

    CommandBuffer& operator=(CommandBuffer&& other) noexcept
    {
        if(this == &other)
            return *this;
        m_recording     = other.m_recording;
        m_commandBuffer = other.m_commandBuffer;

        other.m_commandBuffer = VK_NULL_HANDLE;
        return *this;
    }

private:
    void Allocate(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    bool m_recording;
    VkCommandBuffer m_commandBuffer;
    std::unordered_map<VkPipelineBindPoint, bool> m_isGlobalDescSetBound;
};
