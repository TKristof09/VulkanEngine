#pragma once
#include "Rendering/CommandBuffer.hpp"
#include "VulkanContext.hpp"
#include "vulkan/vulkan_core.h"

class VulkanEvent
{
public:
    VulkanEvent()
    {
        VkEventCreateInfo createInfo = {};
        createInfo.sType             = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
        createInfo.flags             = VK_EVENT_CREATE_DEVICE_ONLY_BIT;
        vkCreateEvent(VulkanContext::GetDevice(), &createInfo, nullptr, &m_event);
    }

    VulkanEvent(const VulkanEvent&)            = delete;
    VulkanEvent(VulkanEvent&&)                 = default;
    VulkanEvent& operator=(const VulkanEvent&) = delete;
    VulkanEvent& operator=(VulkanEvent&&)      = default;


    ~VulkanEvent()
    {
        vkDestroyEvent(VulkanContext::GetDevice(), m_event, nullptr);
    }

    void AddBarrier(VkBufferMemoryBarrier2 barrier)
    {
        m_waitStages |= barrier.dstStageMask;
        m_bufferBarriers.push_back(barrier);
    }

    void AddBarrier(VkImageMemoryBarrier2 barrier)
    {
        m_waitStages |= barrier.dstStageMask;
        m_imageBarriers.push_back(barrier);
    }

    void Set(CommandBuffer& cb)
    {
        VkDependencyInfo info         = {};
        info.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        info.dependencyFlags          = VK_DEPENDENCY_BY_REGION_BIT;
        info.bufferMemoryBarrierCount = static_cast<uint32_t>(m_bufferBarriers.size());
        info.pBufferMemoryBarriers    = m_bufferBarriers.data();
        info.imageMemoryBarrierCount  = static_cast<uint32_t>(m_imageBarriers.size());
        info.pImageMemoryBarriers     = m_imageBarriers.data();


        vkCmdSetEvent2(cb.GetCommandBuffer(), m_event, &info);
    }

    void Reset(CommandBuffer& cb)
    {
        vkCmdResetEvent2(cb.GetCommandBuffer(), m_event, m_waitStages);
    }

    [[nodiscard]] VkEvent GetEvent() const { return m_event; }
    [[nodiscard]] VkDependencyInfo GetDependencyInfo()
    {
        VkDependencyInfo info         = {};
        info.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        info.dependencyFlags          = VK_DEPENDENCY_BY_REGION_BIT;
        info.bufferMemoryBarrierCount = static_cast<uint32_t>(m_bufferBarriers.size());
        info.pBufferMemoryBarriers    = m_bufferBarriers.data();
        info.imageMemoryBarrierCount  = static_cast<uint32_t>(m_imageBarriers.size());
        info.pImageMemoryBarriers     = m_imageBarriers.data();
        return info;
    }

private:
    VkEvent m_event = VK_NULL_HANDLE;
    std::vector<VkBufferMemoryBarrier2> m_bufferBarriers;
    std::vector<VkImageMemoryBarrier2> m_imageBarriers;
    VkPipelineStageFlags2 m_waitStages = 0;
};
