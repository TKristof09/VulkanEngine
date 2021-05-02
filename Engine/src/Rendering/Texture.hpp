#pragma once
#include "Image.hpp"
#include <stb_image.h>
#include <string.h>

class Texture : public Image {

public:
	Texture(): Image(0,0, {}){}
    Texture(const std::string& fileName);
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
	stbi_uc* m_pixels; // dont need to worry about freeing this since it gets freed in the constructor after the loading of the image
};
