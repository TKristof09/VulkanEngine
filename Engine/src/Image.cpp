#include "Image.hpp"
#include "Buffer.hpp"
#include "CommandBuffer.hpp"

bool hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

Image::Image(VkPhysicalDevice gpu, VkDevice device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
             VkImageUsageFlags usage, VkImageLayout layout,VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags, VkSampleCountFlagBits numSamples):
m_device(device),
m_gpu(gpu),
m_width(width),
m_height(height),
m_format(format),
m_image(VK_NULL_HANDLE),
m_imageView(VK_NULL_HANDLE),
m_memory(VK_NULL_HANDLE),
m_layout(layout)
{

    if(numSamples == VK_SAMPLE_COUNT_1_BIT)
		m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;
	else
		m_mipLevels = 1;

    VkImageCreateInfo createInfo            = {};
    createInfo.sType                        = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType                    = VK_IMAGE_TYPE_2D;
    createInfo.extent.width                 = width;
    createInfo.extent.height                = height;
    createInfo.extent.depth                 = 1;
    createInfo.mipLevels                    = m_mipLevels;
    createInfo.arrayLayers                  = 1;
    createInfo.format                       = format;
    createInfo.tiling                       = tiling;
    createInfo.initialLayout                = layout;
    createInfo.usage                        = usage; // TODO possibly add transfer dst bit to not need to specify it when constructing an image
    createInfo.sharingMode                  = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.samples                      = numSamples; // msaa
    createInfo.flags                        = 0;

    VK_CHECK(vkCreateImage(m_device, &createInfo, nullptr, &m_image), "Failed to create texture image");

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, m_image, &memRequirements);
    VkMemoryAllocateInfo allocInfo          = {};
    allocInfo.sType                         = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize                = memRequirements.size;
    allocInfo.memoryTypeIndex               = FindMemoryType(gpu, memRequirements.memoryTypeBits, properties);
    VK_CHECK(vkAllocateMemory(m_device, &allocInfo, nullptr, &m_memory), "Failed to allocate memory for texture");

    vkBindImageMemory(m_device, m_image, m_memory, 0);

    VkImageViewCreateInfo viewCreateInfo    = {};
    viewCreateInfo.sType                    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image                    = m_image;
    viewCreateInfo.viewType                 = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format                   = m_format;
    // stick to default color mapping(probably could leave this as default)
    viewCreateInfo.components.r             = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.g             = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.b             = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.a             = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewCreateInfo.subresourceRange.aspectMask      = aspectFlags;
    viewCreateInfo.subresourceRange.baseMipLevel    = 0;
    viewCreateInfo.subresourceRange.levelCount      = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer  = 0;
    viewCreateInfo.subresourceRange.layerCount      = 1;

    VK_CHECK(vkCreateImageView(m_device, &viewCreateInfo, nullptr, &m_imageView), "Failed to create image views!");
}

Image::Image(VkPhysicalDevice gpu, VkDevice device, std::pair<uint32_t, uint32_t> widthHeight, VkFormat format, VkImageTiling tiling,
             VkImageUsageFlags usage, VkImageLayout layout, VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags, VkSampleCountFlagBits numSamples):
Image(gpu, device, widthHeight.first, widthHeight.second, format, tiling, usage, layout, properties, aspectFlags, numSamples)
{

}


Image::~Image()
{
    vkDestroyImageView(m_device, m_imageView, nullptr);
    vkDestroyImage(m_device, m_image, nullptr);
    vkFreeMemory(m_device, m_memory, nullptr);
}

void Image::TransitionLayout(VkImageLayout newLayout, VkCommandPool commandPool, VkQueue queue)
{
    CommandBuffer commandBuffer(m_device, commandPool);
    commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkImageMemoryBarrier barrier    = {};
    barrier.sType                   = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout               = m_layout;
    barrier.newLayout               = newLayout;
    barrier.srcQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED; // these are for transfering
    barrier.dstQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED; // queue family ownership but we dont want to

    barrier.image                   = m_image;

    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencilComponent(m_format))
        {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    barrier.subresourceRange.baseMipLevel       = 0;
    barrier.subresourceRange.levelCount         = m_mipLevels;
    barrier.subresourceRange.baseArrayLayer     = 0;
    barrier.subresourceRange.layerCount         = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    // TODO need to understand barriers better
    if (m_layout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask   = 0;
        barrier.dstAccessMask   = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage             = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage        = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (m_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask   = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;

        sourceStage             = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage        = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (m_layout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask   = 0;
        barrier.dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage             = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage        = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else
        throw std::invalid_argument("unsupported layout transition!");

    vkCmdPipelineBarrier(commandBuffer.GetCommandBuffer(),
                         sourceStage, destinationStage,
                         0,
                         0, nullptr, 0, nullptr, // these are for other types of barriers
                         1, &barrier);

    commandBuffer.SubmitIdle(queue);
    m_layout = newLayout;
}

void Image::GenerateMipmaps(VkImageLayout newLayout, VkCommandPool commandPool, VkQueue queue)
{

	// Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(m_gpu, m_format, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
	{
		throw std::runtime_error("Texture image format does not support linear blitting!");
	}


    CommandBuffer commandBuffer(m_device, commandPool);
    commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkImageMemoryBarrier barrier            = {};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = m_image;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.subresourceRange.levelCount     = 1;

    int32_t mipWidth    = m_width;
    int32_t mipHeight   = m_height;

    for(uint32_t i = 1; i < m_mipLevels; i++)
    {
        barrier.subresourceRange.baseMipLevel   = i - 1;
        barrier.oldLayout                       = m_layout;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer.GetCommandBuffer(),
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        VkImageBlit blit                    = {}; // blit means copy image and possibly resize
        blit.srcOffsets[0]                  = {0,0,0};
        blit.srcOffsets[1]                  = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel        = i - 1;
        blit.srcSubresource.baseArrayLayer  = 0;
        blit.srcSubresource.layerCount      = 1;

        blit.dstOffsets[0]                  = {0,0,0};
        blit.dstOffsets[1]                  = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel        = i;
        blit.dstSubresource.baseArrayLayer  = 0;
        blit.dstSubresource.layerCount      = 1;

        vkCmdBlitImage(commandBuffer.GetCommandBuffer(),
                       m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_LINEAR);

        // transition the mip images to new layout
        barrier.oldLayout       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout       = newLayout;
        barrier.srcAccessMask   = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer.GetCommandBuffer(),
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }
    // transform the last level
    barrier.subresourceRange.baseMipLevel = m_mipLevels - 1;
    barrier.oldLayout       = m_layout;
    barrier.newLayout       = newLayout;
    barrier.srcAccessMask   = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer.GetCommandBuffer(),
                            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                            0, nullptr,
                            0, nullptr,
                            1, &barrier);

    commandBuffer.SubmitIdle(queue);
}
