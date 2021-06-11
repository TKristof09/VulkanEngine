#pragma once
#include "Image.hpp"

#include <string.h>

class Texture : public Image {
public:
	Texture(): Image(0,0, {}){}
    Texture(const std::string& fileName, VkImageUsageFlags usageFlags);
    ~Texture() = default;

	Texture(Texture&& other) noexcept
		:Image(std::move(other))
	{
	}
	Texture& operator=(Texture&& other) noexcept
	{
		Image::operator=(std::move(other));
		return *this;
	}

private:
    std::pair<uint32_t, uint32_t> LoadFile(const std::string& fileName);
	uint8_t* m_pixels; // dont need to worry about freeing this since it gets freed in the constructor after the loading of the image
};
