#pragma once

#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_glfw.h>
#include <imgui/examples/imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include "CommandBuffer.hpp"
#include "Window.hpp"

struct DebugUIInitInfo
{
	eastl::shared_ptr<Window>	pWindow;
	VkInstance          instance;
    VkPhysicalDevice    gpu;
    VkDevice            device;
    uint32_t            queueFamily;
    VkQueue             queue;
	VkRenderPass        renderPass;
    VkPipelineCache     pipelineCache;
    VkDescriptorPool    descriptorPool;
    uint32_t            imageCount;             // >= MinImageCount
    VkSampleCountFlagBits        msaaSamples;   // >= VK_SAMPLE_COUNT_1_BIT
    const VkAllocationCallbacks* allocator;

	VkCommandPool       commandPool;

};

class DebugUI
{
public:
	DebugUI(DebugUIInitInfo* initInfo);
	~DebugUI();
	void SetupFrame(uint32_t imageIndex, uint32_t subpass, VkFramebuffer framebuffer);
	VkCommandBuffer GetCommandBuffer(uint32_t index) { return m_commandBuffers[index]->GetCommandBuffer(); };
	void SetMinImageCount(VkPresentModeKHR presentMode) { ImGui_ImplVulkan_SetMinImageCount(ImGui_ImplVulkanH_GetMinImageCountFromPresentMode(presentMode)); };
	void ReInit(DebugUIInitInfo* initInfo);
private:
	DebugUIInitInfo m_initInfo;
    bool	m_show_demo_window;
	eastl::vector<eastl::unique_ptr<CommandBuffer>> m_commandBuffers;
};
