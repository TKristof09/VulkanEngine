#pragma once

#include "VulkanContext.hpp"
#include "Shader.hpp"
#include "RenderPass.hpp"

const uint32_t OBJECTS_PER_DESCRIPTOR_CHUNK = 32;
class Pipeline;

struct PipelineCreateInfo
{
	bool useColorBlend		= false;
	bool useMultiSampling	= false;
	bool useDepth			= false;
	bool useStencil			= false;
	bool useTesselation		= false; // not supported yet
	bool useDynamicState	= false; // not supported yet

	VkShaderStageFlags stages;

	VkExtent2D viewportExtent;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	bool depthWriteEnable = false;
	VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;

	bool depthClampEnable = false;

	bool allowDerivatives = false;
	Pipeline* parent = nullptr;

	RenderPass* renderPass = nullptr;
	uint32_t subpass = 0;

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
private:
	friend class RendererSystem;
	friend class MaterialSystem;
	friend class DescriptorSetAllocator;

	void CreateDescriptorSetLayout();
	void CreatePipeline(PipelineCreateInfo createInfo);
	void AllocateDescriptors();

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
};

