#include "Framebuffer.hpp"
#include "RenderPass.hpp"
#include <vulkan/vulkan_core.h>

Framebuffer::Framebuffer(FramebufferCreateInfo createInfo, RenderPass* renderPass):
	m_width(createInfo.width), m_height(createInfo.height), m_fb(VK_NULL_HANDLE)
{
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebufferAttachmentImageInfo> imagelessAttachments;
    if(!createInfo.imageless)
    {

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
    }
    else
    {
        for(auto attachment : createInfo.attachmentDescriptions)
        {
            VkFramebufferAttachmentImageInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO;
            info.height = createInfo.height;
            info.width = createInfo.width;
            info.layerCount = 1;
            info.usage = attachment.imageCreateInfo.usage;
            info.viewFormatCount = 1;
            info.pViewFormats = &attachment.imageCreateInfo.format;
            imagelessAttachments.push_back(info);
        }
    }

	VkFramebufferCreateInfo ci = {};
	ci.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	ci.renderPass		= renderPass->GetRenderPass();
	ci.width			= createInfo.width;
    if(createInfo.imageless)
    {
        VkFramebufferAttachmentsCreateInfo attachments = {};
        attachments.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO;
        attachments.attachmentImageInfoCount = imagelessAttachments.size();
        attachments.pAttachmentImageInfos = imagelessAttachments.data();
        ci.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
        ci.attachmentCount = imagelessAttachments.size();
        ci.pNext = &attachments;
    }
    else
    {
        ci.attachmentCount	= static_cast<uint32_t>(imageViews.size());
        ci.pAttachments		= imageViews.data();
    }
	ci.height			= createInfo.height;
	ci.layers			= 1;

	VK_CHECK(vkCreateFramebuffer(VulkanContext::GetDevice(), &ci, nullptr, &m_fb), "Failed to create framebuffer");
}

Framebuffer::~Framebuffer()
{
	if(m_fb != VK_NULL_HANDLE)
	{
		vkDestroyFramebuffer(VulkanContext::GetDevice(), m_fb, nullptr);
		m_fb = VK_NULL_HANDLE;
	}
};
