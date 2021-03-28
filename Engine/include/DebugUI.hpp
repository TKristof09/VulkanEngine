#pragma once

#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_glfw.h>
#include <imgui/examples/imgui_impl_vulkan.h>
#include <memory>
#include <vector>

#include "VulkanContext.hpp"
#include "CommandBuffer.hpp"
#include "Window.hpp"
#include "RenderPass.hpp"

struct DebugUIInitInfo
{
	std::shared_ptr<Window>	pWindow;
    uint32_t            queueFamily;
    VkQueue             queue;
	RenderPass*         renderPass;
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
private:
    bool	m_show_demo_window;
	//std::vector<std::unique_ptr<CommandBuffer>> m_commandBuffers;
};
