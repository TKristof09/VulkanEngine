#pragma once

#include "Texture.hpp"

class TextureManager
{
public:
    static bool LoadTexture(const std::string& fileName, bool srgb = true);

    static Texture& GetTexture(const std::string& fileName)
    {
        auto it = m_textureMap.find(fileName);
        if(it != m_textureMap.end())
        {
            return it->second;
        }
        LOG_WARN("Texture {0} isn't loaded", fileName);
        return m_textureMap.at("./textures/error.jpg");
    }

    static void FreeTexture(const std::string& fileName)
    {
        auto it = m_textureMap.find(fileName);
        if(it != m_textureMap.end())
        {
            m_textureMap.erase(it);
        }
    }

    static void ClearLoadedTextures()
    {
        m_textureMap.clear();
    }

private:
    static std::unordered_map<std::string, Texture> m_textureMap;
};
