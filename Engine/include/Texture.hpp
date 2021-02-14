#pragma once
#include "Image.hpp"
#include <stb_image.h>
#include <string.h>

class Texture : public Image {

public:
    Texture(const std::string& fileName, VkPhysicalDevice gpu, VkDevice device, VkCommandPool commandPool, VkQueue queue);
    ~Texture() = default;
private:
    std::pair<uint32_t, uint32_t> LoadFile(const std::string& fileName);
    stbi_uc* m_pixels;
};
