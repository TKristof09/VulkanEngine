#include "Framebuffer.hpp"
#include "RenderPass.hpp"
#include <vulkan/vulkan_core.h>

Framebuffer::Framebuffer(FramebufferCreateInfo createInfo, RenderPass* renderPass):
	m_width(createInfo.width), m_height(createInfo.height), m_fb(VK_NULL_HANDLE)
{
	std::vector<VkImageView> imageViews;
	for(auto attachment : createInfo.attachmentDescriptions)
	{
		if(attachment.image)
		{
			imageViews.push_back(attachment.image->GetImageView());
			m_images.push_back(attachment.image);
		}
		else
		{
			m_images.push_back(std::make_shared<Image>(createInfo.width, createInfo.height, attachment.imageCreateInfo));
			imageViews.push_back(m_images.back()->GetImageView());
		}
	}

	VkFramebufferCreateInfo ci = {};
	ci.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	ci.renderPass		= renderPass->GetRenderPass();
	ci.width			= createInfo.width;
	ci.attachmentCount	= static_cast<uint32_t>(imageViews.size());
	ci.pAttachments		= imageViews.data();
	ci.height			= createInfo.height;
	ci.layers			= 1;

	VK_CHECK(vkCreateFramebuffer(VulkanContext::GetDevice(), &ci, nullptr, &m_fb), "Failed to create framebuffer");
}

Framebuffer::~Framebuffer()
{
	vkDestroyFramebuffer(VulkanContext::GetDevice(), m_fb, nullptr);
};
