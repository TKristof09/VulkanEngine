#include "Image.hpp"
#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include <algorithm>

bool hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}


Image::Image(uint32_t width, uint32_t height, ImageCreateInfo createInfo) : m_width(width),
                                                                            m_height(height),
                                                                            m_format(createInfo.format),
                                                                            m_image(createInfo.image),
                                                                            m_layout(VK_IMAGE_LAYOUT_UNDEFINED),
                                                                            m_aspect(createInfo.aspectFlags),
                                                                            m_onlyHandleImageView(createInfo.image != VK_NULL_HANDLE)
{
    if(width == 0 && height == 0)
        return;

    if(createInfo.isCubeMap)
        createInfo.layerCount = 6;

    m_layerCount = createInfo.layerCount;
    m_isCubeMap  = createInfo.isCubeMap;

    if(createInfo.image == VK_NULL_HANDLE)
    {
        if(createInfo.msaaSamples == VK_SAMPLE_COUNT_1_BIT && createInfo.useMips)
            m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;
        else
            m_mipLevels = 1;

        VkImageCreateInfo ci = {};
        ci.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType         = VK_IMAGE_TYPE_2D;
        ci.extent.width      = width;
        ci.extent.height     = height;
        ci.extent.depth      = 1;
        ci.mipLevels         = m_mipLevels;
        ci.arrayLayers       = createInfo.layerCount;
        ci.format            = createInfo.format;
        ci.tiling            = createInfo.tiling;
        ci.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        m_usage              = createInfo.useMips ? createInfo.usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT : createInfo.usage;  // TODO possibly add transfer dst bit to not need to specify it when constructing an image
        ci.usage             = m_usage;
        ci.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
        ci.samples           = createInfo.msaaSamples;  // msaa
        ci.flags             = createInfo.isCubeMap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;


        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage                   = VMA_MEMORY_USAGE_AUTO;
        vmaCreateImage(VulkanContext::GetVmaImageAllocator(), &ci, &allocInfo, &m_image, &m_allocation, nullptr);
    }

    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image                 = m_image;
    if(createInfo.isCubeMap)
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else
        viewCreateInfo.viewType = createInfo.layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format       = m_format;
    // stick to default color mapping(probably could leave this as default)
    viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewCreateInfo.subresourceRange.aspectMask     = createInfo.aspectFlags;
    viewCreateInfo.subresourceRange.baseMipLevel   = 0;
    viewCreateInfo.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount     = createInfo.layerCount;

    m_imageViews.push_back({});
    VK_CHECK(vkCreateImageView(VulkanContext::GetDevice(), &viewCreateInfo, nullptr, &m_imageViews[0]), "Failed to create image views!");

    if(createInfo.layout != VK_IMAGE_LAYOUT_UNDEFINED)
        TransitionLayout(createInfo.layout);

#ifdef VDEBUG
    if(!createInfo.debugName.empty())
    {
        m_debugName                        = createInfo.debugName;
        // TODO load vulkan functions better
        auto* fn                           = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(VulkanContext::GetInstance(), "vkSetDebugUtilsObjectNameEXT"));
        VkDebugUtilsObjectNameInfoEXT info = {};
        info.sType                         = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        info.objectHandle                  = (uint64_t)m_image;
        info.objectType                    = VK_OBJECT_TYPE_IMAGE;
        info.pObjectName                   = createInfo.debugName.c_str();
        fn(VulkanContext::GetDevice(), &info);

        info.objectHandle     = (uint64_t)m_imageViews[0];
        info.objectType       = VK_OBJECT_TYPE_IMAGE_VIEW;
        createInfo.debugName += " image view 0";
        info.pObjectName      = createInfo.debugName.c_str();
        fn(VulkanContext::GetDevice(), &info);
    }
#endif
}

Image::Image(VkExtent2D extent, ImageCreateInfo createInfo) : Image(extent.width, extent.height, createInfo)
{
}

Image::Image(std::pair<uint32_t, uint32_t> widthHeight, ImageCreateInfo createInfo) : Image(widthHeight.first, widthHeight.second, createInfo)
{
}

Image::~Image()
{
    Free();
}

void Image::Free()
{
    if(m_image == VK_NULL_HANDLE || m_imageViews[0] == VK_NULL_HANDLE)
        return;

    for(auto& imageView : m_imageViews)
    {
        vkDestroyImageView(VulkanContext::GetDevice(), imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }

    if(!m_onlyHandleImageView)
    {
        vmaDestroyImage(VulkanContext::GetVmaImageAllocator(), m_image, m_allocation);
        m_image = VK_NULL_HANDLE;
    }
}

VkImageView Image::CreateImageView(uint32_t mip)
{
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image                 = m_image;
    if(m_isCubeMap)
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else
        viewCreateInfo.viewType = m_layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format       = m_format;
    // stick to default color mapping(probably could leave this as default)
    viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewCreateInfo.subresourceRange.aspectMask     = m_aspect;
    viewCreateInfo.subresourceRange.baseMipLevel   = mip;
    viewCreateInfo.subresourceRange.levelCount     = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount     = m_layerCount;

    m_imageViews.push_back({});
    VK_CHECK(vkCreateImageView(VulkanContext::GetDevice(), &viewCreateInfo, nullptr, &m_imageViews.back()), "Failed to create image views!");

#ifdef VDEBUG
    if(m_debugName.empty())
    {
        // TODO load vulkan functions better
        auto* fn                           = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(VulkanContext::GetInstance(), "vkSetDebugUtilsObjectNameEXT"));
        VkDebugUtilsObjectNameInfoEXT info = {};
        info.sType                         = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        fn(VulkanContext::GetDevice(), &info);

        info.objectHandle = (uint64_t)m_imageViews[0];
        info.objectType   = VK_OBJECT_TYPE_IMAGE_VIEW;
        std::string name  = m_debugName + " image view " + std::to_string(m_imageViews.size() - 1);
        info.pObjectName  = name.c_str();
        fn(VulkanContext::GetDevice(), &info);
    }
#endif


    return m_imageViews.back();
}


// TODO add layer support for other functions

void Image::TransitionLayout(VkImageLayout newLayout)
{
    CommandBuffer commandBuffer;
    commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkImageMemoryBarrier barrier = {};
    barrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout            = m_layout;
    barrier.newLayout            = newLayout;
    barrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;  // these are for transfering
    barrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;  // queue family ownership but we dont want to

    barrier.image = m_image;

    barrier.subresourceRange.aspectMask = m_aspect;

    if(hasStencilComponent(m_format))
    {
        barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    // TODO need to understand barriers better
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    sourceStage           = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    destinationStage      = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    vkCmdPipelineBarrier(commandBuffer.GetCommandBuffer(),
                         sourceStage, destinationStage,
                         0,
                         0, nullptr, 0, nullptr,  // these are for other types of barriers
                         1, &barrier);

    commandBuffer.SubmitIdle(VulkanContext::GetGraphicsQueue());
    m_layout = newLayout;
}

void Image::GenerateMipmaps(VkImageLayout newLayout)
{
    if(m_mipLevels == 1)
    {
        LOG_WARN("Image::GenerateMipmaps called on an image that has only one mip level");
        TransitionLayout(newLayout);
        return;
    }
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(VulkanContext::GetPhysicalDevice(), m_format, &formatProperties);
    if(!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        throw std::runtime_error("Texture image format does not support linear blitting!");
    }


    CommandBuffer commandBuffer;
    commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkImageMemoryBarrier barrier            = {};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = m_image;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = m_layerCount;
    barrier.subresourceRange.levelCount     = 1;

    // Transition first mip level to transfer source for read during blit
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.oldLayout                     = m_layout;
    barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer.GetCommandBuffer(),
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    int32_t mipWidth  = m_width;
    int32_t mipHeight = m_height;

    for(uint32_t i = 1; i < m_mipLevels; i++)
    {
        barrier.subresourceRange.baseMipLevel = i;
        barrier.oldLayout                     = m_layout;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer.GetCommandBuffer(),
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        VkImageBlit blit                   = {};  // blit means copy image and possibly resize
        blit.srcOffsets[0]                 = {0, 0, 0};
        blit.srcOffsets[1]                 = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel       = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount     = m_layerCount;

        blit.dstOffsets[0]                 = {0, 0, 0};
        blit.dstOffsets[1]                 = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel       = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount     = m_layerCount;

        vkCmdBlitImage(commandBuffer.GetCommandBuffer(),
                       m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_LINEAR);

        barrier.subresourceRange.baseMipLevel = i;
        barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer.GetCommandBuffer(),
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);


        if(mipWidth > 1)
            mipWidth /= 2;
        if(mipHeight > 1)
            mipHeight /= 2;
    }

    // transition the mip images to new layout
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount   = m_mipLevels;
    barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout                     = newLayout;
    barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer.GetCommandBuffer(),
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    commandBuffer.SubmitIdle(VulkanContext::GetGraphicsQueue());

    m_layout = newLayout;
}
