#include "CommandBuffer.hpp"
#include <limits>

CommandBuffer::CommandBuffer(Queue queue, VkCommandBufferLevel level)
    : m_recording(false),
      m_queue(queue),
      m_commandBuffer(VK_NULL_HANDLE)
{
    m_recording = false;
    if(queue.familyIndex == VulkanContext::GetTransferQueue().familyIndex)
        m_commandPool = VulkanContext::GetTransferCommandPool();
    else
        m_commandPool = VulkanContext::GetGraphicsCommandPool();

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level                       = level;
    allocInfo.commandPool                 = m_commandPool;
    allocInfo.commandBufferCount          = 1;

    VK_CHECK(vkAllocateCommandBuffers(VulkanContext::GetDevice(), &allocInfo, &m_commandBuffer), "Failed to allocate command buffers!");
}

CommandBuffer::~CommandBuffer()
{
    Free();
}


void CommandBuffer::Free()
{
    if(m_commandBuffer != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(VulkanContext::GetDevice(), m_commandPool, 1, &m_commandBuffer);
        m_commandBuffer = VK_NULL_HANDLE;
    }
}

void CommandBuffer::Begin(VkCommandBufferUsageFlags usage)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = usage;

    VK_CHECK(vkBeginCommandBuffer(m_commandBuffer, &beginInfo), "Failed to begin recording command buffer!");
    m_recording = true;
}
void CommandBuffer::Begin(VkCommandBufferUsageFlags usage, VkCommandBufferInheritanceInfo inheritanceInfo)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = usage;
    beginInfo.pInheritanceInfo         = &inheritanceInfo;  // this is ignore if its a primary command buffer

    VK_CHECK(vkBeginCommandBuffer(m_commandBuffer, &beginInfo), "Failed to begin recording command buffer!");
    m_recording = true;
}

void CommandBuffer::End()
{
    if(!m_recording)
        return;

    VK_CHECK(vkEndCommandBuffer(m_commandBuffer), "Failed to record command buffer!");
    m_recording = false;
    m_isGlobalDescSetBound.clear();
}

void CommandBuffer::SubmitIdle()
{
    if(m_recording)
        End();

    VkSubmitInfo submitInfo       = {};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &m_commandBuffer;


    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;

    VK_CHECK(vkCreateFence(VulkanContext::GetDevice(), &fenceInfo, nullptr, &fence), "Failed to create fence");
    VK_CHECK(vkResetFences(VulkanContext::GetDevice(), 1, &fence), "Failed to reset fence");
    VK_CHECK(vkQueueSubmit(m_queue.queue, 1, &submitInfo, fence), "Failed to submit queue");
    VK_CHECK(vkWaitForFences(VulkanContext::GetDevice(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max()), "Failed to wait for fence");
    vkDestroyFence(VulkanContext::GetDevice(), fence, nullptr);
}
void CommandBuffer::Submit(VkSemaphore waitSemaphore, VkPipelineStageFlags waitStage, VkSemaphore signalSemaphore, VkFence fence)
{
    PROFILE_FUNCTION();
    if(m_recording)
        End();

    VkSubmitInfo submitInfo       = {};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &m_commandBuffer;

    if(waitSemaphore != VK_NULL_HANDLE && waitStage != 0)
    {
        // at which stage to wait for the semaphores

        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores    = &waitSemaphore;
        submitInfo.pWaitDstStageMask  = &waitStage;
    }

    if(signalSemaphore != VK_NULL_HANDLE)
    {
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &signalSemaphore;
    }
    if(fence != VK_NULL_HANDLE)
    {
        VK_CHECK(vkResetFences(VulkanContext::GetDevice(), 1, &fence), "Failed to reset fence");
    }

    VK_CHECK(vkQueueSubmit(m_queue.queue, 1, &submitInfo, fence), "Failed to submit command buffer" + std::to_string((uint64_t)m_commandBuffer));
}

void CommandBuffer::Submit(const std::vector<VkSemaphore>& waitSemaphores, const std::vector<VkPipelineStageFlags>& waitStages, const std::vector<VkSemaphore>& signalSemaphores, VkFence fence)
{
    if(m_recording)
        End();

    VkSubmitInfo submitInfo       = {};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &m_commandBuffer;


    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    submitInfo.pWaitSemaphores    = waitSemaphores.data();
    submitInfo.pWaitDstStageMask  = waitStages.data();


    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    submitInfo.pSignalSemaphores    = signalSemaphores.data();

    if(fence != VK_NULL_HANDLE)
    {
        VK_CHECK(vkResetFences(VulkanContext::GetDevice(), 1, &fence), "Failed to reset fence");
    }

    VK_CHECK(vkQueueSubmit(m_queue.queue, 1, &submitInfo, fence), "Failed to submit command buffer");
}

void CommandBuffer::Reset()
{
    VK_CHECK(vkResetCommandBuffer(m_commandBuffer, 0), "Failed to reset command buffer!");
    m_recording = false;
}

void CommandBuffer::BindGlobalDescSet(VkPipelineBindPoint bindPoint, VkPipelineLayout layout)
{
    if(m_isGlobalDescSetBound[bindPoint])
        return;

    VkDescriptorSet descSet = VulkanContext::GetGlobalDescSet();
    vkCmdBindDescriptorSets(m_commandBuffer, bindPoint, layout, 0, 1, &descSet, 0, nullptr);
    m_isGlobalDescSetBound[bindPoint] = true;
}
