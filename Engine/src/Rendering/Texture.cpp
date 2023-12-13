#include "Texture.hpp"
#include "Buffer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <filesystem>


Texture Texture::Create(const std::string& fileName, bool srgb)
{
    int width, height, channels;
    std::filesystem::path f = fileName;

    stbi_uc* pixels = stbi_load(std::filesystem::absolute(f).string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if(channels != 4)
        LOG_WARN("Texture {0} has {1} channels, but is loaded with 4 channels", fileName.substr(fileName.find_last_of("/") + 1), channels);
    if(!pixels)
        throw std::runtime_error("Failed to load texture image!");


    ImageCreateInfo imageCI = {};
    imageCI.format          = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    imageCI.usage           = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCI.aspectFlags     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCI.debugName       = fileName;

    Texture texture(width, height, imageCI);

    VkDeviceSize imageSize = width * height * 4;  // 8 bit per channel, 4 channels
    Buffer stagingBuffer(imageSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         true);

    stagingBuffer.Fill(pixels, imageSize);
    stbi_image_free(pixels);

    texture.TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    stagingBuffer.CopyToImage(texture.m_image, width, height);
    texture.GenerateMipmaps(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

    return texture;
}

Texture Texture::CreateHDR(const std::string& fileName)
{
    int width, height, channels;
    std::filesystem::path f = fileName;

    float* pixels = stbi_loadf(std::filesystem::absolute(f).string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if(channels != 4)
        LOG_WARN("Texture {0} has {1} channels, but is loaded with 4 channels", fileName.substr(fileName.find_last_of("/") + 1), channels);
    if(!pixels)
        throw std::runtime_error("Failed to load texture image!");


    ImageCreateInfo imageCI = {};
    imageCI.format          = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageCI.usage           = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCI.aspectFlags     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCI.debugName       = fileName;

    Texture texture(width, height, imageCI);

    VkDeviceSize imageSize = width * height * 4 * 4;  // 32 bit per channel, 4 channels
    Buffer stagingBuffer(imageSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         true);

    stagingBuffer.Fill(pixels, imageSize);
    stbi_image_free(pixels);

    texture.TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    stagingBuffer.CopyToImage(texture.m_image, width, height);
    texture.GenerateMipmaps(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

    return texture;
}
