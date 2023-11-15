#include "Texture.hpp"
#include "Buffer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <filesystem>
Texture::Texture(const std::string& fileName, VkImageUsageFlags usageFlags, bool hdr) : Image(LoadFile(fileName, hdr),
                                                                                              {hdr ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM,
                                                                                               VK_IMAGE_TILING_OPTIMAL,
                                                                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | usageFlags,
                                                                                               VK_IMAGE_LAYOUT_UNDEFINED,
                                                                                               VK_IMAGE_ASPECT_COLOR_BIT,
                                                                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT})
{
    VkDeviceSize imageSize = m_width * m_height * 4;
    if(hdr)
        imageSize *= 4; // because 32bit per channel instead of 8bit for normal textures
    Buffer stagingBuffer(imageSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         true);

    stagingBuffer.Fill(m_pixels, imageSize);
    stbi_image_free(m_pixels);
    m_pixels = nullptr;

    TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    stagingBuffer.CopyToImage(m_image, m_width, m_height);
    GenerateMipmaps(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
}

std::pair<uint32_t, uint32_t> Texture::LoadFile(const std::string& fileName, bool hdr)
{
    int width, height, channels;
    std::filesystem::path f = fileName;
    if(hdr)
        m_pixels = stbi_loadf(std::filesystem::absolute(f).string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    else
        m_pixels = stbi_load(std::filesystem::absolute(f).string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    LOG_INFO("Cwd: {0}", std::filesystem::current_path().string());
    LOG_INFO("File: {0}", std::filesystem::absolute(f));

    m_width  = static_cast<uint32_t>(width);
    m_height = static_cast<uint32_t>(height);
    if(channels != 4)
        LOG_WARN("Texture {0} has {1} channels, but is loaded with 4 channels", fileName.substr(fileName.find_last_of("/") + 1), channels);
    if(!m_pixels)
        throw std::runtime_error("Failed to load texture image!");

    return std::pair<uint32_t, uint32_t>(m_width, height);
}
