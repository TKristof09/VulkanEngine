#include "Pipeline.hpp"
#include "ECS/CoreComponents/Mesh.hpp"

Pipeline::Pipeline(const std::string& shaderName, PipelineCreateInfo createInfo, uint16_t priority)
	:m_name(shaderName), m_priority(priority), m_isGlobal(createInfo.isGlobal), m_renderPass(createInfo.renderPass)
{

	if(createInfo.stages & VK_SHADER_STAGE_VERTEX_BIT)
	{
		Shader vs(m_name, VK_SHADER_STAGE_VERTEX_BIT);
		m_shaders.push_back(vs);
	}
	if(createInfo.stages & VK_SHADER_STAGE_FRAGMENT_BIT)
	{
		Shader fs(m_name, VK_SHADER_STAGE_FRAGMENT_BIT);
		m_shaders.push_back(fs);
	}


	CreateDescriptorSetLayout(createInfo.useVariableDescriptorCount);
	CreatePipeline(createInfo);

}

Pipeline::~Pipeline()
{
	for(uint32_t i = 0; i <= m_descSetLayouts.size(); ++i)
		vkDestroyDescriptorSetLayout(VulkanContext::GetDevice(), m_descSetLayouts[i], nullptr);
	vkDestroyPipeline(VulkanContext::GetDevice(), m_pipeline, nullptr);
	vkDestroyPipelineLayout(VulkanContext::GetDevice(), m_layout, nullptr);

}

void Pipeline::CreatePipeline(PipelineCreateInfo createInfo)
{

	std::vector<VkPipelineShaderStageCreateInfo> stagesCI;
	for(Shader& shader : m_shaders)
	{
		VkPipelineShaderStageCreateInfo ci = {};
		ci.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		ci.stage	= shader.m_stage;
		ci.pName	= "main";
		ci.module	= shader.GetShaderModule();

		stagesCI.push_back(ci);

	}
	// ##################### VERTEX INPUT #####################
	auto bindingDescription = GetVertexBindingDescription();
	auto attribDescriptions = GetVertexAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInput = {}; // vertex info hardcoded for the moment
	vertexInput.sType							= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount	= 1;
	vertexInput.pVertexBindingDescriptions		= &bindingDescription;
	vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribDescriptions.size());
	vertexInput.pVertexAttributeDescriptions	= attribDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo assembly = {};
	assembly.sType	  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// ##################### VIEWPORT #####################
	VkViewport viewport = {};
	viewport.width		= (float)createInfo.viewportExtent.width;
	viewport.height		= -(float)createInfo.viewportExtent.height;
	viewport.x			= 0.f;
	viewport.y			= (float)createInfo.viewportExtent.height;
	viewport.minDepth	= 0.0f;
	viewport.maxDepth	= 1.0f;

	VkRect2D scissor = {};
	scissor.offset	= {0, 0};
	scissor.extent	= createInfo.viewportExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pViewports	= &viewport;
	viewportState.viewportCount = 1;
	viewportState.pScissors		= &scissor;
	viewportState.scissorCount	= 1;

	// ##################### RASTERIZATION #####################
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType					= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.cullMode					= VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace				= VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.polygonMode				= VK_POLYGON_MODE_FILL;
	rasterizer.depthClampEnable			= createInfo.depthClampEnable;
	rasterizer.rasterizerDiscardEnable	= false;
	rasterizer.lineWidth				= 1.0f;
	rasterizer.depthBiasEnable			= false;

	VkPipelineMultisampleStateCreateInfo multisample = {};
	multisample.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.sampleShadingEnable		= VK_TRUE;
	multisample.minSampleShading		= 0.2f; // closer to 1 is smoother
	multisample.rasterizationSamples	= createInfo.msaaSamples;


	// ##################### COLOR BLEND #####################
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask			= VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
	colorBlendAttachment.blendEnable			= true;
	colorBlendAttachment.srcColorBlendFactor	= VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor	= VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp			= VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor	= VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor	= VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp			= VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlend = {};
	colorBlend.sType			= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.logicOpEnable	= false;
	colorBlend.attachmentCount	= 1;
	colorBlend.pAttachments		= &colorBlendAttachment;


	// ##################### DEPTH #####################
	VkPipelineDepthStencilStateCreateInfo depthStencil  = {};
	depthStencil.sType					= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable		= createInfo.useDepth;
	depthStencil.depthWriteEnable		= createInfo.depthWriteEnable;
	depthStencil.depthCompareOp			= createInfo.depthCompareOp; // not OP_LESS because we have a depth prepass
	depthStencil.depthBoundsTestEnable	= VK_TRUE;
	depthStencil.minDepthBounds			= 0.0f;
	depthStencil.maxDepthBounds			= 1.0f;
	depthStencil.stencilTestEnable		= createInfo.useStencil;


	// ##################### PUSH CONSTANTS #####################
	std::vector<VkPushConstantRange> pcRanges;
	for(auto shader : m_shaders)
	{
		if(shader.HasPushConstants())
		{
			VkPushConstantRange pcRange = {};
			pcRange.stageFlags	= shader.m_stage;
			pcRange.offset		= shader.m_pushConstants.offset;
			pcRange.size		= shader.m_pushConstants.size;

			pcRanges.push_back(pcRange);
		}
	}


	// ##################### LAYOUT #####################
	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType					= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount			= m_numDescSets; //m_descSetLayouts.size(); temporary see TODO in hpp
	layoutCreateInfo.pSetLayouts			= m_descSetLayouts.data();
	layoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pcRanges.size());
	layoutCreateInfo.pPushConstantRanges	= pcRanges.data();

	VK_CHECK(vkCreatePipelineLayout(VulkanContext::GetDevice(), &layoutCreateInfo, nullptr, &m_layout), "Failed to create pipeline layout");

	// ##################### PIPELINE #####################
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType					= VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount				= static_cast<uint32_t>(stagesCI.size());
	pipelineInfo.pStages				= stagesCI.data();
	pipelineInfo.pVertexInputState		= &vertexInput;
	pipelineInfo.pInputAssemblyState	= &assembly;
	pipelineInfo.pViewportState			= &viewportState;
	pipelineInfo.pRasterizationState	= &rasterizer;
	if(createInfo.useMultiSampling)
		pipelineInfo.pMultisampleState	= &multisample;
	if(createInfo.useColorBlend)
		pipelineInfo.pColorBlendState	= &colorBlend;
	pipelineInfo.pDepthStencilState		= &depthStencil;
	if(createInfo.allowDerivatives)
		pipelineInfo.flags				= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	else if(createInfo.parent)
	{
		pipelineInfo.flags				= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
		pipelineInfo.basePipelineHandle = createInfo.parent->m_pipeline;
		pipelineInfo.basePipelineIndex  = -1;
	}
	//pipelineInfo.pDynamicState = &dynamicState;

	pipelineInfo.layout		= m_layout;
	pipelineInfo.renderPass = createInfo.renderPass->GetRenderPass();
	pipelineInfo.subpass	= createInfo.subpass;

	VK_CHECK(vkCreateGraphicsPipelines(VulkanContext::GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline), "Failed to create graphics pipeline");

}

void Pipeline::CreateDescriptorSetLayout(bool useVariableDescriptorCount)
{
	std::array<std::vector<VkDescriptorSetLayoutBinding>, 4> bindings;
	for(auto& shader : m_shaders)
	{
		for(auto& [name, bufferInfo] : shader.m_uniformBuffers)
		{
			VkDescriptorSetLayoutBinding layoutBinding  = {};
			layoutBinding.binding                       = bufferInfo.binding;
			layoutBinding.descriptorType                = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			layoutBinding.descriptorCount               = bufferInfo.count;
			layoutBinding.stageFlags                    = bufferInfo.stage;
			layoutBinding.pImmutableSamplers            = nullptr; // this is for texture samplers

			bindings[bufferInfo.set].push_back(layoutBinding);

		}
		for(auto& [name, textureInfo] : shader.m_textures)
		{
			VkDescriptorSetLayoutBinding samplerLayoutBinding  = {};
			samplerLayoutBinding.binding                       = textureInfo.binding;
			samplerLayoutBinding.descriptorType                = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			if(textureInfo.isLast && useVariableDescriptorCount)
			{
				textureInfo.count = textureInfo.count * OBJECTS_PER_DESCRIPTOR_CHUNK;
			}

			samplerLayoutBinding.descriptorCount				= textureInfo.count;
			samplerLayoutBinding.stageFlags                    	= textureInfo.stage;
			samplerLayoutBinding.pImmutableSamplers            	= nullptr;

			bindings[textureInfo.set].push_back(samplerLayoutBinding);

		}
	}
	for(uint32_t i = 0; i < bindings.size(); ++i)
	{
		if(bindings[i].size() == 0)
		{
			m_numDescSets = i; // temporary see TODO in the hpp file
			break;
		}
		VkDescriptorSetLayoutCreateInfo createInfo  = {};
		createInfo.sType                            = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount                     = static_cast<uint32_t>(bindings[i].size());
		createInfo.pBindings                        = bindings[i].data();
		createInfo.pNext = nullptr;

		// need to define these outside of the if statement otherwise they get cleaned up before we create the layout
		VkDescriptorSetLayoutBindingFlagsCreateInfo flags = {};
		std::vector<VkDescriptorBindingFlags> bindingFlags(bindings[i].size() - 1, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);

		if(useVariableDescriptorCount && i == 1) // TODO for now we only allow this in set 1 which is the set for materials
		{
			flags.sType				= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;

			bindingFlags.push_back(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);

			flags.pBindingFlags		= bindingFlags.data();
			flags.bindingCount		= bindingFlags.size();
			createInfo.pNext		= &flags;
		}
		VK_CHECK(vkCreateDescriptorSetLayout(VulkanContext::GetDevice(), &createInfo, nullptr, &m_descSetLayouts[i]), "Failed to create descriptor set layout");
	}
}
