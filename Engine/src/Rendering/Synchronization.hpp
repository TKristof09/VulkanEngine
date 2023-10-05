#pragma once
#include "Rendering/CommandBuffer.hpp"
#include "VulkanContext.hpp"
#include "vulkan/vulkan_core.h"

class VulkanEvent
{
public:
    VulkanEvent() = default;

    VulkanEvent(const VulkanEvent&)            = delete;
    VulkanEvent(VulkanEvent&&)                 = default;
    VulkanEvent& operator=(const VulkanEvent&) = delete;
    VulkanEvent& operator=(VulkanEvent&&)      = default;


    ~VulkanEvent()
    {
        if(m_event != VK_NULL_HANDLE)
            vkDestroyEvent(VulkanContext::GetDevice(), m_event, nullptr);
    }

    void Allocate()
    {
        VkEventCreateInfo createInfo = {};
        createInfo.sType             = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
        createInfo.flags             = VK_EVENT_CREATE_DEVICE_ONLY_BIT;
        vkCreateEvent(VulkanContext::GetDevice(), &createInfo, nullptr, &m_event);
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

    // @brief Remove a barrier corresponding to the buffer (if exists)
    void RemoveBarrier(VkBuffer buffer)
    {
        for(auto it = m_bufferBarriers.begin(); it != m_bufferBarriers.end(); ++it)
        {
            if(it->buffer == buffer)
            {
                m_bufferBarriers.erase(it);
                return;
            }
        }
    }

    // @brief Remove a barrier corresponding to the image (if exists)
    void RemoveBarrier(VkImage image)
    {
        for(auto it = m_imageBarriers.begin(); it != m_imageBarriers.end(); ++it)
        {
            if(it->image == image)
            {
                m_imageBarriers.erase(it);
                return;
            }
        }
    }
    // @brief Try to find a barrier corresponding to buffer
    // @return true if barrier is found, false otherwise. If found, it is copied into outBarrier
    bool GetBarrier(VkBuffer buffer, VkBufferMemoryBarrier2& outBarrier)
    {
        for(auto& b : m_bufferBarriers)
        {
            if(b.buffer == buffer)
            {
                outBarrier = b;
                return true;
            }
        }
        return false;
    }
    // @brief Try to find a barrier corresponding to image
    // @return true if barrier is found, false otherwise. If found, it is copied into outBarrier
    bool GetBarrier(VkImage image, VkImageMemoryBarrier2& outBarrier)
    {
        for(auto& b : m_imageBarriers)
        {
            if(b.image == image)
            {
                outBarrier = b;
                return true;
            }
        }
        return false;
    }

    // @brief Check if image's layout gets transitioned by this event
    [[nodiscard]] bool IsImageLayoutTransition(VkImage image)
    {
        for(auto& barrier : m_imageBarriers)
        {
            if(barrier.image == image)
                return barrier.oldLayout != barrier.newLayout;
        }
        return false;
    }

    void Set(CommandBuffer& cb)
    {
        VkDependencyInfo info         = {};
        info.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        info.dependencyFlags          = 0;
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
        info.dependencyFlags          = 0;
        info.bufferMemoryBarrierCount = static_cast<uint32_t>(m_bufferBarriers.size());
        info.pBufferMemoryBarriers    = m_bufferBarriers.data();
        info.imageMemoryBarrierCount  = static_cast<uint32_t>(m_imageBarriers.size());
        info.pImageMemoryBarriers     = m_imageBarriers.data();
        return info;
    }

    [[nodiscard]] std::vector<VkBufferMemoryBarrier2> GetBufferBarriers() const { return m_bufferBarriers; }
    [[nodiscard]] std::vector<VkImageMemoryBarrier2> GetImageBarriers() const { return m_imageBarriers; }

    [[nodiscard]] bool Empty() const { return m_bufferBarriers.empty() && m_imageBarriers.empty(); }

private:
    VkEvent m_event = VK_NULL_HANDLE;
    std::vector<VkBufferMemoryBarrier2> m_bufferBarriers;
    std::vector<VkImageMemoryBarrier2> m_imageBarriers;
    VkPipelineStageFlags2 m_waitStages = 0;
};
