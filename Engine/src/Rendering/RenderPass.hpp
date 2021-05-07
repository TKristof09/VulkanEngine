#pragma once
#include "VulkanContext.hpp"
#include "Framebuffer.hpp"
#include <memory>

enum class PipelineType;

enum class RenderPassAttachmentType
{
	COLOR, DEPTH, RESOLVE, INPUT
};
struct RenderPassAttachment
{
	uint32_t subpass = 0;
	PipelineType pipelineType;
	
	bool preserve = false;

	RenderPassAttachmentType type;

	VkFormat format;
	VkAttachmentLoadOp loadOp;
	VkAttachmentStoreOp storeOp;


	VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImageLayout internalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkClearValue clearValue = {};

};

struct RenderPassCreateInfo
{
	std::vector<RenderPassAttachment> attachments; // need to be in the same order as in shader
	std::vector<VkSubpassDependency> dependencies;
	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	uint32_t subpassCount = 1;
};

class RenderPass
{
public:
	RenderPass() = default;
	RenderPass(RenderPassCreateInfo createInfo);
	~RenderPass();
	void Destroy();
	
	void AddFramebuffer(FramebufferCreateInfo createInfo);

	VkRenderPass GetRenderPass() const { return m_renderPass; }
	VkRenderPassBeginInfo GetBeginInfo(uint32_t framebufferIndex = 0);
	std::shared_ptr<Image> GetFramebufferImage(uint32_t framebufferIndex, uint32_t imageIndex) { return m_framebuffers[framebufferIndex]->GetImage(imageIndex); }
	VkFramebuffer GetFramebuffer(uint32_t index) { return m_framebuffers[index]->GetFramebuffer(); }

	// prevent copying
	RenderPass(const RenderPass&) = delete;
	RenderPass& operator=(const RenderPass&) = delete;
	RenderPass& operator=(RenderPass&& other)
	{
		m_renderPass = other.m_renderPass;
		if(other.m_framebuffers.size() > 0)
			m_framebuffers = std::move(other.m_framebuffers);
		else
			m_framebuffers.clear();
		m_clearValues = other.m_clearValues;

		other.m_renderPass = VK_NULL_HANDLE;
		return *this;
	}
private:
	void CreateRenderPass(RenderPassCreateInfo createInfo);

	VkRenderPass m_renderPass;
	std::vector<std::unique_ptr<Framebuffer>> m_framebuffers;
	std::vector<VkClearValue> m_clearValues;

};
