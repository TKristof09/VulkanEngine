#include "CommandBuffer.hpp"
#include <limits>

CommandBuffer::CommandBuffer(VkCommandBufferLevel level):
m_running(false),
m_commandBuffer(VK_NULL_HANDLE)
{
    Allocate(level);
}

void CommandBuffer::Allocate(VkCommandBufferLevel level)
{
    m_running = false;

    VkCommandBufferAllocateInfo allocInfo       = {};
    allocInfo.sType                             = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool                       = VulkanContext::GetCommandPool();
    allocInfo.level                             = level;
    allocInfo.commandBufferCount                = 1;

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
        vkFreeCommandBuffers(VulkanContext::GetDevice(), VulkanContext::GetCommandPool(), 1, &m_commandBuffer);
    	m_commandBuffer = VK_NULL_HANDLE;
    }
}

void CommandBuffer::Begin(VkCommandBufferUsageFlags usage)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = usage;

    VK_CHECK(vkBeginCommandBuffer(m_commandBuffer, &beginInfo), "Failed to begin recording command buffer!");
    m_running = true;
}
void CommandBuffer::Begin(VkCommandBufferUsageFlags usage, VkCommandBufferInheritanceInfo inheritanceInfo)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = usage;
	beginInfo.pInheritanceInfo = &inheritanceInfo; //this is ignore if its a primary command buffer

    VK_CHECK(vkBeginCommandBuffer(m_commandBuffer, &beginInfo), "Failed to begin recording command buffer!");
    m_running = true;
}

void CommandBuffer::End()
{
    if(!m_running)
        return;

    VK_CHECK(vkEndCommandBuffer(m_commandBuffer), "Failed to record command buffer!");
    m_running = false;
}

void CommandBuffer::SubmitIdle(VkQueue queue)
{
    if(m_running)
        End();

    VkSubmitInfo submitInfo             = {};
    submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount       = 1;
    submitInfo.pCommandBuffers          = &m_commandBuffer;


    VkFenceCreateInfo fenceInfo         = {};
    fenceInfo.sType                     = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;

    VK_CHECK(vkCreateFence(VulkanContext::GetDevice(), &fenceInfo, nullptr, &fence), "Failed to create fence");
    VK_CHECK(vkResetFences(VulkanContext::GetDevice(), 1, &fence), "Failed to reset fence");
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence), "Failed to submit queue");
    VK_CHECK(vkWaitForFences(VulkanContext::GetDevice(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max()), "Failed to wait for fence");
    vkDestroyFence(VulkanContext::GetDevice(), fence, nullptr);

}
void CommandBuffer::Submit(VkQueue queue, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStage, VkSemaphore signalSemaphore, VkFence fence)
{
    if(m_running)
        End();

    VkSubmitInfo submitInfo             = {};
    submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount       = 1;
    submitInfo.pCommandBuffers          = &m_commandBuffer;

    if(waitSemaphore != VK_NULL_HANDLE && waitStage != 0)
    {

        // at which stage to wait for the semaphores

        submitInfo.waitSemaphoreCount       = 1;
        submitInfo.pWaitSemaphores          = &waitSemaphore;
        submitInfo.pWaitDstStageMask        = &waitStage;

    }

    if(signalSemaphore != VK_NULL_HANDLE)
    {
        submitInfo.signalSemaphoreCount     = 1;
        submitInfo.pSignalSemaphores        = &signalSemaphore;
    }
    if(fence != VK_NULL_HANDLE)
    {
        VK_CHECK(vkResetFences(VulkanContext::GetDevice(), 1, &fence), "Failed to reset fence");
    }

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence), "Failed to submit command buffer");
}

void CommandBuffer::Submit(VkQueue queue, const std::vector<VkSemaphore>& waitSemaphores, const std::vector<VkPipelineStageFlags>& waitStages, const std::vector<VkSemaphore>& signalSemaphores, VkFence fence)
{
    if(m_running)
        End();

    VkSubmitInfo submitInfo             = {};
    submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount       = 1;
    submitInfo.pCommandBuffers          = &m_commandBuffer;


	submitInfo.waitSemaphoreCount       = waitSemaphores.size();
	submitInfo.pWaitSemaphores          = waitSemaphores.data();
	submitInfo.pWaitDstStageMask        = waitStages.data();


	submitInfo.signalSemaphoreCount     = signalSemaphores.size();
	submitInfo.pSignalSemaphores        = signalSemaphores.data();

    if(fence != VK_NULL_HANDLE)
    {
        VK_CHECK(vkResetFences(VulkanContext::GetDevice(), 1, &fence), "Failed to reset fence");
    }

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence), "Failed to submit command buffer");
}
