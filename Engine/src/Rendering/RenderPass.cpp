#include "RenderPass.hpp"
#include "vulkan/vulkan_core.h"


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

	std::vector<VkAttachmentDescription2> attachments;
    attachments.reserve(createInfo.attachments.size()); // use reserve because we skip resolve attachments in case msaaSamples is 1
	std::vector<std::vector<VkAttachmentReference2>> colorRefs(createInfo.subpassCount);
	std::vector<std::vector<VkAttachmentReference2>> resolveRefs(createInfo.subpassCount);
	std::vector<std::vector<VkAttachmentReference2>> inputRefs(createInfo.subpassCount);
	std::vector<VkAttachmentReference2> depthRefs(createInfo.subpassCount);
	std::vector<std::vector<uint32_t>> preserveRefs(createInfo.subpassCount);
	std::vector<VkAttachmentReference2> depthResolveRefs(createInfo.subpassCount);
	std::vector<VkSubpassDescriptionDepthStencilResolve> depthResolves(createInfo.subpassCount);

	std::vector<VkSubpassDescription2> subpasses(createInfo.subpassCount);
	for(uint32_t currentSubpass = 0; currentSubpass < createInfo.subpassCount; ++currentSubpass)
	{
		subpasses[currentSubpass].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;

		bool useDepth = false;
		bool useResolve = false;


		depthResolves[currentSubpass].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;

		for(auto attachment : createInfo.attachments)
		{

			if(attachment.subpass != currentSubpass)
				continue;
			if(createInfo.msaaSamples == VK_SAMPLE_COUNT_1_BIT && (attachment.type == RenderPassAttachmentType::COLOR_RESOLVE || attachment.type == RenderPassAttachmentType::DEPTH_RESOLVE))
                continue;

			VkAttachmentDescription2 desc = {};
			desc.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
			desc.format = attachment.format;

			if(attachment.type == RenderPassAttachmentType::COLOR_RESOLVE || attachment.type == RenderPassAttachmentType::DEPTH_RESOLVE)
				desc.samples = VK_SAMPLE_COUNT_1_BIT;
			else
				desc.samples = createInfo.msaaSamples;

			desc.loadOp = attachment.loadOp;
			desc.storeOp = attachment.storeOp;
			desc.stencilLoadOp = attachment.stencilLoadOp;
			desc.stencilStoreOp = attachment.stencilStoreOp;
			desc.initialLayout = attachment.initialLayout;
			desc.finalLayout = attachment.finalLayout;

			attachments.push_back(desc);

			VkAttachmentReference2 ref = {};
			ref.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
			ref.attachment = i;
			ref.layout = attachment.internalLayout;



			switch(attachment.type)
			{
				case RenderPassAttachmentType::COLOR: colorRefs[currentSubpass].push_back(ref); break;
				case RenderPassAttachmentType::DEPTH: depthRefs[currentSubpass] = ref; useDepth = true; break;
				case RenderPassAttachmentType::COLOR_RESOLVE: resolveRefs[currentSubpass].push_back(ref); useResolve = true; break;
				case RenderPassAttachmentType::INPUT: inputRefs[currentSubpass].push_back(ref); break;
				case RenderPassAttachmentType::DEPTH_RESOLVE:
					depthResolves[currentSubpass].depthResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
					depthResolves[currentSubpass].stencilResolveMode = VK_RESOLVE_MODE_NONE;
					depthResolveRefs[currentSubpass]  = ref;
					depthResolves[currentSubpass].pDepthStencilResolveAttachment = &depthResolveRefs[currentSubpass];
					subpasses[currentSubpass].pNext = &depthResolves[currentSubpass];
					useResolve = true;
					break;

			}

			if(attachment.preserve)
				preserveRefs[currentSubpass].push_back(i);

			m_clearValues.push_back(attachment.clearValue);

			i++;

		}


		if(!useResolve && createInfo.msaaSamples != VK_SAMPLE_COUNT_1_BIT)
			LOG_WARN("Not using resolve attachment despite having MSAA sample count > 1");

		VkSubpassDescription2& subpass = subpasses[currentSubpass];
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

	VkRenderPassCreateInfo2 ci = {};
	ci.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
	ci.attachmentCount  = static_cast<uint32_t>(attachments.size());
	ci.pAttachments     = attachments.data();
	ci.subpassCount     = subpasses.size();
	ci.pSubpasses       = subpasses.data();

	ci.dependencyCount = createInfo.dependencies.size();
	ci.pDependencies = createInfo.dependencies.data()	;

	VK_CHECK(vkCreateRenderPass2(VulkanContext::GetDevice(), &ci, nullptr, &m_renderPass), "Failed to create render pass");

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
