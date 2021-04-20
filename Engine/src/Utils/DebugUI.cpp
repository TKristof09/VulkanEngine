#include "Utils/DebugUI.hpp"

DebugUI::DebugUI(DebugUIInitInfo initInfo) :
	m_show_demo_window(true)
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForVulkan(initInfo.pWindow->GetWindow(), true);

	ImGui_ImplVulkan_InitInfo imguiInitInfo = {};
	imguiInitInfo.Instance = VulkanContext::GetInstance();
	imguiInitInfo.PhysicalDevice = VulkanContext::GetPhysicalDevice();
	imguiInitInfo.Device = VulkanContext::GetDevice();
	imguiInitInfo.QueueFamily = initInfo.queueFamily;
	imguiInitInfo.Queue = initInfo.queue;
	imguiInitInfo.PipelineCache = initInfo.pipelineCache;
	imguiInitInfo.DescriptorPool = initInfo.descriptorPool;
	imguiInitInfo.Allocator = initInfo.allocator;
	imguiInitInfo.MinImageCount = 2;
	imguiInitInfo.ImageCount = initInfo.imageCount;
	imguiInitInfo.MSAASamples = initInfo.msaaSamples;

	ImGui_ImplVulkan_Init(&imguiInitInfo, initInfo.renderPass->GetRenderPass());

	// upload font textures
	CommandBuffer cb;
	cb.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	ImGui_ImplVulkan_CreateFontsTexture(cb.GetCommandBuffer());
	cb.SubmitIdle(initInfo.queue);
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	/*
	m_commandBuffers.resize(initInfo.imageCount);
	for(size_t i = 0; i < m_commandBuffers.size(); i++)
	{
		m_commandBuffers[i] = std::make_unique<CommandBuffer>(VulkanContext::GetDevice(), initInfo.commandPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
	}
	*/
}

DebugUI::~DebugUI()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void DebugUI::ReInit(DebugUIInitInfo initInfo)
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
	ImGui::StyleColorsDark();

	ImGui_ImplVulkan_InitInfo imguiInitInfo = {};
	imguiInitInfo.Instance = VulkanContext::GetInstance();
	imguiInitInfo.PhysicalDevice = VulkanContext::GetPhysicalDevice();
	imguiInitInfo.Device = VulkanContext::GetDevice();
	imguiInitInfo.QueueFamily = initInfo.queueFamily;
	imguiInitInfo.Queue = initInfo.queue;
	imguiInitInfo.PipelineCache = initInfo.pipelineCache;
	imguiInitInfo.DescriptorPool = initInfo.descriptorPool;
	imguiInitInfo.Allocator = initInfo.allocator;
	imguiInitInfo.MinImageCount = 2;
	imguiInitInfo.ImageCount = initInfo.imageCount;
	imguiInitInfo.MSAASamples = initInfo.msaaSamples;


	ImGui_ImplVulkan_Init(&imguiInitInfo, initInfo.renderPass->GetRenderPass());
	CommandBuffer cb;
	cb.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	ImGui_ImplVulkan_CreateFontsTexture(cb.GetCommandBuffer());
	cb.SubmitIdle(initInfo.queue);
	ImGui_ImplVulkan_DestroyFontUploadObjects();


}
void DebugUI::Draw(CommandBuffer* cb)
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	for (auto [i, window] : m_windows)
	{
		window->Update();
	}
	if (m_show_demo_window)
		ImGui::ShowDemoWindow(&m_show_demo_window);

	ImGui::Render();
	

	/*VkCommandBufferInheritanceInfo inheritanceInfo = {};
	inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceInfo.renderPass = renderPass->GetRenderPass();
	inheritanceInfo.subpass = subpass;
	inheritanceInfo.framebuffer = renderPass->GetFramebuffer(imageIndex);

	m_commandBuffers[imageIndex]->Begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, inheritanceInfo);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), GetCommandBuffer(imageIndex));
	m_commandBuffers[imageIndex]->End();*/

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb->GetCommandBuffer());

}

void DebugUI::AddWindow(DebugUIWindow* window)
{
	window->m_index = m_index++;
	window->m_debugUI = this;
	m_windows[m_index] = window;
}
void DebugUI::RemoveWindow(uint32_t index)
{
	auto it = m_windows.find(index);
	if (it != m_windows.end())
		m_windows.erase(it);
}