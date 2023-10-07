#pragma once
#include "VulkanContext.hpp"
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

struct ImageCreateInfo
{
    VkFormat format;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags usage;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageAspectFlags aspectFlags;
    VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    bool useMips = true;

    VkImage image = VK_NULL_HANDLE;  // just to make it so that we can create an Image from the swapchain images
};
class Image
{
public:
    Image(uint32_t width, uint32_t height, ImageCreateInfo createInfo);
    Image(VkExtent2D extent, ImageCreateInfo createInfo);
    Image(std::pair<uint32_t, uint32_t> widthHeight, ImageCreateInfo createInfo);

    Image(const Image& other) = delete;

    Image(Image&& other) noexcept
        : m_mipLevels(other.m_mipLevels),
          m_width(other.m_width),
          m_height(other.m_height),
          m_image(other.m_image),
          m_imageView(other.m_imageView),
          m_format(other.m_format),
          m_memory(other.m_memory),
          m_layout(other.m_layout),
          m_aspect(other.m_aspect),
          m_onlyHandleImageView(other.m_onlyHandleImageView),
          m_allocation(other.m_allocation)
    {
        other.m_image     = VK_NULL_HANDLE;
        other.m_imageView = VK_NULL_HANDLE;
    }

    Image& operator=(const Image& other) = delete;

    Image& operator=(Image&& other) noexcept
    {
        if(this == &other)
            return *this;
        m_mipLevels           = other.m_mipLevels;
        m_width               = other.m_width;
        m_height              = other.m_height;
        m_image               = other.m_image;
        m_imageView           = other.m_imageView;
        m_format              = other.m_format;
        m_memory              = other.m_memory;
        m_layout              = other.m_layout;
        m_aspect              = other.m_aspect;
        m_onlyHandleImageView = other.m_onlyHandleImageView;
        m_allocation          = other.m_allocation;

        other.m_image     = VK_NULL_HANDLE;
        other.m_imageView = VK_NULL_HANDLE;
        return *this;
    }

    virtual ~Image();
    void Free();
    void TransitionLayout(VkImageLayout newLayout);
    void GenerateMipmaps(VkImageLayout newLayout);

    const VkImageView GetImageView() const { return m_imageView; }
    const VkImageLayout GetLayout() const { return m_layout; }
    const VkImage GetImage() const { return m_image; }
    const VkFormat GetFormat() const { return m_format; }
    const uint32_t GetMipLevels() const { return m_mipLevels; }
    const uint32_t GetWidth() const { return m_width; }
    const uint32_t GetHeight() const { return m_height; }

protected:
    uint32_t m_mipLevels;

    uint32_t m_width;
    uint32_t m_height;
    VkImage m_image;
    VkImageView m_imageView;
    VkFormat m_format;
    VkDeviceMemory m_memory;
    VkImageLayout m_layout;
    VkImageAspectFlags m_aspect;

    bool m_onlyHandleImageView = false;

    VmaAllocation m_allocation;
};
