#pragma once
#include "Image.hpp"

#include <string>

class Texture : public Image
{
public:
    static Texture Create(const std::string& fileName, bool srgb = true);
    static Texture CreateHDR(const std::string& fileName);
    ~Texture() override = default;

    Texture(Texture&& other) noexcept
        : Image(std::move(other))
    {
    }
    Texture& operator=(Texture&& other) noexcept
    {
        Image::operator=(std::move(other));
        return *this;
    }

private:
    Texture(uint32_t width, uint32_t height, ImageCreateInfo ci) : Image(width, height, ci) {}
};
