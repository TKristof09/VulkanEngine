#pragma once

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <memory>

#include "VulkanContext.hpp"
#include "CommandBuffer.hpp"
#include "Window.hpp"
#include "RenderPass.hpp"
#include "Utils/DebugUIElements.hpp"

struct DebugUIInitInfo
{
    std::shared_ptr<Window>	pWindow;
    uint32_t            queueFamily;
    VkQueue             queue;
    RenderPass* renderPass;
    VkPipelineCache     pipelineCache;
    VkDescriptorPool    descriptorPool;
    uint32_t            imageCount;             // >= MinImageCount
    VkSampleCountFlagBits        msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    const VkAllocationCallbacks* allocator = nullptr;

};

class DebugUI
{
public:
    DebugUI(DebugUIInitInfo initInfo);
    ~DebugUI();
    void SetupFrame(CommandBuffer* cb/*uint32_t imageIndex, uint32_t subpass, RenderPass* renderPass*/);
    //VkCommandBuffer GetCommandBuffer(uint32_t index) { return m_commandBuffers[index]->GetCommandBuffer(); };
    void SetMinImageCount(VkPresentModeKHR presentMode) { ImGui_ImplVulkan_SetMinImageCount(ImGui_ImplVulkanH_GetMinImageCountFromPresentMode(presentMode)); };
    void ReInit(DebugUIInitInfo initInfo);


    void AddWindow(DebugUIWindow* window);
    void RemoveWindow(uint32_t index);
private:
    bool	m_show_demo_window;
    unsigned int m_index;
    std::unordered_map<uint32_t, DebugUIWindow*> m_windows;
    //std::vector<std::unique_ptr<CommandBuffer>> m_commandBuffers;
};
