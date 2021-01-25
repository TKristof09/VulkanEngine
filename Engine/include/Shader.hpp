#pragma once
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <string>
#include <vector>
#include "UniformBuffer.hpp"
#include "Texture.hpp"

class Shader
{
public:
    Shader(const std::string& filename, VkPipelineStageFlags stage);
    VkShaderModule GetShaderModule() const { return m_shaderModule; };

private:
	friend class RendererSystem;

	struct UniformBufferInfo
	{
		VkPipelineStageFlags stage;

		size_t size;
		uint32_t location;
		uint32_t binding;
		uint32_t set;
		uint32_t count;
	};
	struct TextureInfo
	{
		VkPipelineStageFlags stage;

		uint32_t binding;
		// uint32_t set; is this a thing?
		uint32_t count;
	};
	VkShaderModule m_shaderModule;
	std::unordered_map<std::string, UniformBufferInfo> m_uniformBuffers;
	std::unordered_map<std::string, TextureInfo> m_Textures;

};
