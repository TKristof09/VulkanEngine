#pragma once

#include "VulkanContext.hpp"
#include "Image.hpp"

class RenderPass;
struct FramebufferImageInfo
{
	std::shared_ptr<Image> image; // we use this if it isn't nullptr

	ImageCreateInfo imageCreateInfo; // create image based on this if image is nullptr

};
struct FramebufferCreateInfo
{
	uint32_t width, height;
	// uint32_t layers;

	std::vector<FramebufferImageInfo> attachmentDescriptions; // respect the order of the attachments

};
class Framebuffer
{
public:
	Framebuffer(FramebufferCreateInfo createInfo, RenderPass* renderPass);
	~Framebuffer();
	VkFramebuffer GetFramebuffer() const { return m_fb; }
	std::shared_ptr<Image> GetImage(uint32_t index)
	{
		assert(index < m_images.size());
		return m_images[index];
	}
	std::pair<uint32_t, uint32_t> GetWidthHeight() const { return {m_width, m_height}; }

private:
	VkFramebuffer m_fb;
	uint32_t m_width, m_height;
	std::vector<std::shared_ptr<Image>> m_images;
};
