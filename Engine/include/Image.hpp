#pragma once
#include <vulkan/vulkan.h>
class Image {

public:
    Image(VkPhysicalDevice gpu, VkDevice device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
          VkImageUsageFlags usage, VkImageLayout layout, VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
    Image(VkPhysicalDevice gpu, VkDevice device, std::pair<uint32_t, uint32_t> widthHeight, VkFormat format, VkImageTiling tiling,
          VkImageUsageFlags usage, VkImageLayout layout, VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
    virtual ~Image();
    void TransitionLayout(VkImageLayout newLayout, VkCommandPool commandPool, VkQueue queue);
    void GenerateMipmaps(VkImageLayout newLayout, VkCommandPool commandPool, VkQueue queue);
    const VkImageView& GetImageView() const
    {
        return m_imageView;
    }
    const VkFormat& GetFormat() const
    {
        return m_format;
    }
    const uint32_t GetMipLevels() const
    {
        return m_mipLevels;
    }

protected:
    uint32_t        m_mipLevels;
    VkDevice        m_device;
	VkPhysicalDevice m_gpu;
    uint32_t        m_width;
    uint32_t        m_height;
    VkImage         m_image;
    VkImageView     m_imageView;
    VkFormat        m_format;
    VkDeviceMemory  m_memory;
    VkImageLayout   m_layout;

};
