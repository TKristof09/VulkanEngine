#include "TextureManager.hpp"

std::unordered_map<std::string, Texture> TextureManager::m_textureMap = {};
bool TextureManager::LoadTexture(const std::string& fileName, bool srgb)
{
    auto it = m_textureMap.find(fileName);
    if(it != m_textureMap.end())
        return true;
    else
    {
        LOG_INFO("Loading texture: {0}", fileName);
        if(fileName.ends_with(".hdr"))
            m_textureMap.insert_or_assign(fileName, Texture::CreateHDR(fileName));
        else
            m_textureMap.insert_or_assign(fileName, Texture::Create(fileName, srgb));
        return true;
    }

    // TODO failure return (need to implement in Texture)
}
