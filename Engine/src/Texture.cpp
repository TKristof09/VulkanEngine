#define STB_IMAGE_IMPLEMENTATION
#include "Texture.hpp"
#include "Buffer.hpp"
Texture::Texture(const std::string& fileName, VkPhysicalDevice gpu, VkDevice device, VkCommandPool commandPool, VkQueue queue):
Image(LoadFile(fileName), {VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_ASPECT_COLOR_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT} )
{

    VkDeviceSize imageSize = m_width * m_height * 4;

    Buffer stagingBuffer(gpu, device, imageSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    stagingBuffer.Fill(m_pixels, imageSize);
    stbi_image_free(m_pixels);
    m_pixels = nullptr;

    TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandPool, queue);
    stagingBuffer.CopyToImage(m_image, m_width, m_height, commandPool, queue);
    GenerateMipmaps(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandPool, queue);



}

std::pair<uint32_t, uint32_t> Texture::LoadFile(const std::string& fileName)
{
    int width, height, channels;
    m_pixels = stbi_load(fileName.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    m_width = static_cast<uint32_t>(width);
    m_height = static_cast<uint32_t>(height);
	if(channels != 4)
		LOG_WARN("Texture {0} has {1} channels, but is loaded with 4 channels", fileName.substr(fileName.find_last_of("/") + 1), channels);
    if(!m_pixels)
        throw std::runtime_error("Failed to load texture image!");

    return std::pair<uint32_t, uint32_t>(m_width, height);
}
