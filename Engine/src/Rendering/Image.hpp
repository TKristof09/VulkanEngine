#pragma once
#include "VulkanContext.hpp"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>


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

    uint8_t layerCount = 1;

    bool isCubeMap = false;  // this implicitly sets layerCount to 6

    VkImage image = VK_NULL_HANDLE;  // just to make it so that we can create an Image from the swapchain images

    std::string debugName;
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
          m_imageViews(std::move(other.m_imageViews)),
          m_format(other.m_format),
          m_layout(other.m_layout),
          m_aspect(other.m_aspect),
          m_usage(other.m_usage),
          m_onlyHandleImageView(other.m_onlyHandleImageView),
          m_allocation(other.m_allocation),
          m_sampledSlot(other.m_sampledSlot)
    {
        other.m_image = VK_NULL_HANDLE;
    }

    Image& operator=(const Image& other) = delete;

    Image& operator=(Image&& other) noexcept
    {
        if(this == &other)
            return *this;
        if(m_image != VK_NULL_HANDLE)
            Free();

        m_mipLevels           = other.m_mipLevels;
        m_width               = other.m_width;
        m_height              = other.m_height;
        m_image               = other.m_image;
        m_imageViews          = std::move(other.m_imageViews);
        m_format              = other.m_format;
        m_layout              = other.m_layout;
        m_aspect              = other.m_aspect;
        m_usage               = other.m_usage;
        m_onlyHandleImageView = other.m_onlyHandleImageView;
        m_allocation          = other.m_allocation;
        m_sampledSlot         = other.m_sampledSlot;

        other.m_image = VK_NULL_HANDLE;
        return *this;
    }

    virtual ~Image();
    void Free();
    void TransitionLayout(VkImageLayout newLayout);
    void GenerateMipmaps(VkImageLayout newLayout);

    VkImageView CreateImageView(uint32_t mip);

    const VkImageView GetImageView(uint32_t index = 0) const { return m_imageViews[index]; }
    const VkImageLayout GetLayout() const { return m_layout; }
    const VkImage GetImage() const { return m_image; }
    const VkFormat GetFormat() const { return m_format; }
    const VkImageUsageFlags GetUsage() const { return m_usage; }

    const uint32_t GetMipLevels() const { return m_mipLevels; }
    const uint32_t GetWidth() const { return m_width; }
    const uint32_t GetHeight() const { return m_height; }

    void SetSampledSlot(int32_t slot) { m_sampledSlot = slot; }
    const int32_t GetSampledSlot() const { return m_sampledSlot; }

    void SetStorageSlot(int32_t slot) { m_storageSlot = slot; }
    const int32_t GetStorageSlot() const { return m_storageSlot; }

protected:
    uint32_t m_mipLevels;

    uint32_t m_width;
    uint32_t m_height;
    VkImage m_image;
    std::vector<VkImageView> m_imageViews;
    VkFormat m_format;
    VkImageLayout m_layout;
    VkImageAspectFlags m_aspect;
    VkImageUsageFlags m_usage;
    uint32_t m_layerCount = 1;
    bool m_isCubeMap      = false;
    std::string m_debugName;

    int32_t m_sampledSlot = -1;
    int32_t m_storageSlot = -1;

    bool m_onlyHandleImageView = false;

    VmaAllocation m_allocation;
};
