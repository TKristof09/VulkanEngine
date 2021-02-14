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
    Shader(const std::string& filename, VkShaderStageFlagBits stage);
private:
	friend class RendererSystem;
	friend class Pipeline;

	VkShaderModule GetShaderModule() const { return m_shaderModule; };
	bool HasPushConstants() const { return m_pushConstants.used; };

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
		uint32_t set;
		uint32_t count;

		bool isLast = false;
	};
	struct PushConstantsInfo
	{
		bool used = false;
		uint32_t size;
		uint32_t offset;
	};
	VkShaderModule m_shaderModule;
	VkShaderStageFlagBits m_stage;
	PushConstantsInfo m_pushConstants;
	std::unordered_map<std::string, UniformBufferInfo> m_uniformBuffers;
	std::unordered_map<std::string, TextureInfo> m_Textures;

};
