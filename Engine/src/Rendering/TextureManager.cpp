#include "TextureManager.hpp"

std::unordered_map<std::string, Texture> TextureManager::m_textureMap = {};
bool TextureManager::LoadTexture(const std::string& fileName, VkImageUsageFlags usageFlags)
{
	auto it = m_textureMap.find(fileName);
	if(it != m_textureMap.end())
		return true;
	else
	{
		LOG_INFO("Loading texture: {0}", fileName);
		m_textureMap[fileName] = Texture(fileName, usageFlags);
		return true;
	}

	// TODO failure return (need to implement in Texture)
}


