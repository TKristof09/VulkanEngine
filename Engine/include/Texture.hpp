#pragma once
#include "Image.hpp"
#include <stb_image.h>
#include <EASTL/string.h>

class Texture : public Image {

public:
    Texture(const eastl::string& fileName, VkPhysicalDevice gpu, VkDevice device, VkCommandPool commandPool, VkQueue queue);
    ~Texture();
private:
    std::pair<uint32_t, uint32_t> LoadFile(const eastl::string& fileName);
    stbi_uc* m_pixels;
};
