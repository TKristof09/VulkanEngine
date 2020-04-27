#pragma once
#include "Image.hpp"
#include <stb_image.h>
class Texture : public Image {

public:
    Texture(const eastl::string& fileName, VkPhysicalDevice gpu, VkDevice device, VkCommandPool commandPool, VkQueue queue);
    ~Texture();
private:
    eastl::pair<uint32_t, uint32_t> LoadFile(const eastl::string& fileName);
    stbi_uc* m_pixels;
};
