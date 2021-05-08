#pragma once

#include "VulkanContext.hpp"
#include "Shader.hpp"
#include "RenderPass.hpp"

const uint32_t OBJECTS_PER_DESCRIPTOR_CHUNK = 32;
class Pipeline;

enum class PipelineType{
	GRAPHICS,
	COMPUTE
};

struct PipelineCreateInfo
{
	PipelineType type;
	
	bool allowDerivatives = false;
	Pipeline* parent = nullptr;

	RenderPass* renderPass = nullptr;
	uint32_t subpass = 0;


	// for GRAPHICS
	bool useColorBlend    = false;
	bool useMultiSampling = false;
	bool useDepth         = false;
	bool useStencil       = false;
	bool useTesselation   = false; // not supported yet
	bool useDynamicState  = false; // not supported yet

	VkShaderStageFlags stages = 0;
	VkExtent2D viewportExtent = {};

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	bool depthWriteEnable = false;
	VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;

	bool depthClampEnable = false;
	
	bool isGlobal = false;
};

class Pipeline
{
public:
	Pipeline(const std::string& shaderName, PipelineCreateInfo createInfo, uint16_t priority=std::numeric_limits<uint16_t>::max());
	~Pipeline();

	friend bool operator<(const Pipeline& lhs, const Pipeline& rhs)
	{
		if(lhs.m_priority == rhs.m_priority)
			if(lhs.m_renderPass == rhs.m_renderPass)
				return lhs.m_name < rhs.m_name;
			else
				return lhs.m_renderPass < rhs.m_renderPass;
		else
			return lhs.m_priority < rhs.m_priority;
	}

	Pipeline(const Pipeline& other) = delete;

	Pipeline(Pipeline&& other) noexcept
		: m_priority(other.m_priority),
		  m_isGlobal(other.m_isGlobal),
		  m_name(std::move(other.m_name)),
		  m_shaders(std::move(other.m_shaders)),
		  m_renderPass(other.m_renderPass),
		  m_pipeline(other.m_pipeline),
		  m_layout(other.m_layout),
		  m_descSetLayouts(std::move(other.m_descSetLayouts)),
		  m_numDescSets(other.m_numDescSets),
		  m_descSets(std::move(other.m_descSets)),
		  m_pushConstants(std::move(other.m_pushConstants)),
		  m_uniformBuffers(std::move(other.m_uniformBuffers)),
		  m_textures(std::move(other.m_textures)),
		  m_storageImages(std::move(other.m_storageImages)),
		  m_storageBuffers(std::move(other.m_storageBuffers))
	{
		other.m_pipeline = VK_NULL_HANDLE;
	}

	Pipeline& operator=(const Pipeline& other) = delete;

	Pipeline& operator=(Pipeline&& other) noexcept
	{
		if(this == &other)
			return *this;
		m_priority       = other.m_priority;
		m_isGlobal       = other.m_isGlobal;
		m_name           = std::move(other.m_name);
		m_shaders        = std::move(other.m_shaders);
		m_renderPass     = other.m_renderPass;
		m_pipeline       = other.m_pipeline;
		m_layout         = other.m_layout;
		m_descSetLayouts = std::move(other.m_descSetLayouts);
		m_numDescSets    = other.m_numDescSets;
		m_descSets       = std::move(other.m_descSets);
		m_pushConstants  = std::move(other.m_pushConstants);
		m_uniformBuffers = std::move(other.m_uniformBuffers);
		m_textures       = std::move(other.m_textures);
		m_storageImages  = std::move(other.m_storageImages);
		m_storageBuffers = std::move(other.m_storageBuffers);

		other.m_pipeline = VK_NULL_HANDLE;
		return *this;
	}

private:
	friend class Renderer;
	friend class MaterialSystem;
	friend class DescriptorSetAllocator;
	friend class Shader;

	void CreateDescriptorSetLayout();
	void CreateGraphicsPipeline(PipelineCreateInfo createInfo);
	void CreateComputePipeline(PipelineCreateInfo createInfo);
	void AllocateDescriptors();

	void MergeDuplicateBindings();

	uint16_t				m_priority;
	bool					m_isGlobal;
	std::string				m_name;
	std::vector<Shader>		m_shaders;
	RenderPass*				m_renderPass;
	VkPipeline				m_pipeline;
	VkPipelineLayout		m_layout;
	std::array<VkDescriptorSetLayout, 4> m_descSetLayouts;
	uint8_t					m_numDescSets; //TODO in the future this will not be needed as every shader will be required to use 4 sets

	std::unordered_map<std::string, VkDescriptorSet> m_descSets;

	struct BufferInfo
	{
		VkPipelineStageFlags stages;

		size_t size;
		
		uint32_t binding;
		uint32_t set;
		uint32_t count;
	};
	struct TextureInfo
	{
		VkPipelineStageFlags stages;

		uint32_t binding;
		uint32_t set;
		uint32_t count;
	};
	struct PushConstantInfo
	{
		
		VkShaderStageFlags stages;
		uint32_t size;
		uint32_t offset;
	};
	std::unordered_map<std::string, PushConstantInfo> m_pushConstants;
	std::unordered_map<std::string, BufferInfo> m_uniformBuffers;
	std::unordered_map<std::string, TextureInfo> m_textures;
	std::unordered_map<std::string, TextureInfo> m_storageImages;
	std::unordered_map<std::string, BufferInfo> m_storageBuffers;
};

