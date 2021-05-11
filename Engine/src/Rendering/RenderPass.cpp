#include "RenderPass.hpp"


RenderPass::RenderPass(RenderPassCreateInfo createInfo)
{
	CreateRenderPass(createInfo);
}

RenderPass::~RenderPass()
{
	Destroy();
}

void RenderPass::Destroy()
{
	if(m_renderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(VulkanContext::GetDevice(), m_renderPass, nullptr);
		m_framebuffers.clear();
		m_renderPass = VK_NULL_HANDLE;
	}

}

void RenderPass::CreateRenderPass(RenderPassCreateInfo createInfo)
{

	uint32_t i = 0;

	std::vector<VkAttachmentDescription> attachments(createInfo.attachments.size());
	std::vector<std::vector<VkAttachmentReference>> colorRefs(createInfo.subpassCount);
	std::vector<std::vector<VkAttachmentReference>> resolveRefs(createInfo.subpassCount);
	std::vector<std::vector<VkAttachmentReference>> inputRefs(createInfo.subpassCount);
	std::vector<VkAttachmentReference> depthRefs(createInfo.subpassCount);
	std::vector<std::vector<uint32_t>> preserveRefs(createInfo.subpassCount);

	std::vector<VkSubpassDescription> subpasses(createInfo.subpassCount);
	for(uint32_t currentSubpass = 0; currentSubpass < createInfo.subpassCount; ++currentSubpass)
	{
		bool useDepth = false;
		bool useResolve = false;

		for(auto attachment : createInfo.attachments)
		{

			if(attachment.subpass != currentSubpass)
				continue;
			VkAttachmentDescription desc = {};
			desc.format = attachment.format;
			if(attachment.type == RenderPassAttachmentType::RESOLVE)
				desc.samples = VK_SAMPLE_COUNT_1_BIT;
			else
				desc.samples = createInfo.msaaSamples;
			desc.loadOp = attachment.loadOp;
			desc.storeOp = attachment.storeOp;
			desc.stencilLoadOp = attachment.stencilLoadOp;
			desc.stencilStoreOp = attachment.stencilStoreOp;
			desc.initialLayout = attachment.initialLayout;
			desc.finalLayout = attachment.finalLayout;

			attachments[i] = desc;

			VkAttachmentReference ref = {};
			ref.attachment = i;
			ref.layout = attachment.internalLayout;
			switch(attachment.type)
			{
				case RenderPassAttachmentType::COLOR: colorRefs[currentSubpass].push_back(ref); break;
				case RenderPassAttachmentType::DEPTH: depthRefs[currentSubpass] = ref; useDepth = true; break;
				case RenderPassAttachmentType::RESOLVE: resolveRefs[currentSubpass].push_back(ref); useResolve = true; break;
				case RenderPassAttachmentType::INPUT: inputRefs[currentSubpass].push_back(ref); break;
			}

			if(attachment.preserve)
				preserveRefs[currentSubpass].push_back(i);

			m_clearValues.push_back(attachment.clearValue);

			i++;

		}

		if(colorRefs[currentSubpass].size() != resolveRefs[currentSubpass].size() && useResolve)
		{
			LOG_ERROR("Not the same amount of color and resolve attachments");
			assert(false);
		}
		if(!useResolve && createInfo.msaaSamples != VK_SAMPLE_COUNT_1_BIT)
			LOG_WARN("Not using resolve attachment despite having MSAA sample count > 1");

		VkSubpassDescription& subpass = subpasses[currentSubpass];
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		subpass.colorAttachmentCount = colorRefs[currentSubpass].size();
		subpass.pColorAttachments = colorRefs[currentSubpass].data();;
		if(useDepth)
			subpass.pDepthStencilAttachment = &depthRefs[currentSubpass];

		if(useResolve)
			subpass.pResolveAttachments = resolveRefs[currentSubpass].data();

		subpass.inputAttachmentCount = inputRefs[currentSubpass].size();
		subpass.pInputAttachments = inputRefs[currentSubpass].data();

		subpass.preserveAttachmentCount = preserveRefs[currentSubpass].size();
		subpass.pPreserveAttachments	= preserveRefs[currentSubpass].data();

	}

	VkRenderPassCreateInfo ci = {};
	ci.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	ci.attachmentCount  = static_cast<uint32_t>(attachments.size());
	ci.pAttachments     = attachments.data();
	ci.subpassCount     = subpasses.size();
	ci.pSubpasses       = subpasses.data();

	ci.dependencyCount = createInfo.dependencies.size();
	ci.pDependencies = createInfo.dependencies.data()	;

	VK_CHECK(vkCreateRenderPass(VulkanContext::GetDevice(), &ci, nullptr, &m_renderPass), "Failed to create render pass");

}
void RenderPass::AddFramebuffer(FramebufferCreateInfo createInfo)
{
	m_framebuffers.emplace_back(std::make_unique<Framebuffer>(createInfo, this));
}

VkRenderPassBeginInfo RenderPass::GetBeginInfo(uint32_t framebufferIndex)
{
	VkRenderPassBeginInfo info = {};
	info.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.renderPass			= m_renderPass;
	info.framebuffer		= m_framebuffers[framebufferIndex]->GetFramebuffer();
	info.renderArea.offset	= {0, 0};

	auto [width, height] = m_framebuffers[framebufferIndex]->GetWidthHeight();
	info.renderArea.extent.width	= width;
	info.renderArea.extent.height	= height;

	info.clearValueCount	= m_clearValues.size();
	info.pClearValues		= m_clearValues.data();

	return info;
}
