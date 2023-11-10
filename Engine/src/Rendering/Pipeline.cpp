#include "Pipeline.hpp"
#include "ECS/CoreComponents/Mesh.hpp"
#include "Rendering/VulkanContext.hpp"

Pipeline::Pipeline(const std::string& shaderName, PipelineCreateInfo createInfo, uint16_t priority)
    : m_name(shaderName),
      m_priority(priority),
      m_isGlobal(createInfo.isGlobal),
      m_viewMask(createInfo.viewMask)
{
    if(createInfo.type == PipelineType::GRAPHICS)
    {
        if(createInfo.stages & VK_SHADER_STAGE_VERTEX_BIT)
        {
            m_shaders.emplace_back(m_name, VK_SHADER_STAGE_VERTEX_BIT, this);
        }
        if(createInfo.stages & VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            m_shaders.emplace_back(m_name, VK_SHADER_STAGE_FRAGMENT_BIT, this);
        }
    }
    else
    {
        m_shaders.emplace_back(m_name, VK_SHADER_STAGE_COMPUTE_BIT, this);
    }


    if(createInfo.type == PipelineType::GRAPHICS)
        CreateGraphicsPipeline(createInfo);
    else
        CreateComputePipeline(createInfo);

    for(auto& shader : m_shaders)
    {
        shader.DestroyShaderModule();
    }
}

Pipeline::~Pipeline()
{
    if(m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(VulkanContext::GetDevice(), m_pipeline, nullptr);
        vkDestroyPipelineLayout(VulkanContext::GetDevice(), m_layout, nullptr);

        m_pipeline = VK_NULL_HANDLE;
    }
}

void Pipeline::CreateGraphicsPipeline(const PipelineCreateInfo& createInfo)
{
    std::vector<VkPipelineShaderStageCreateInfo> stagesCI;
    for(Shader& shader : m_shaders)
    {
        VkPipelineShaderStageCreateInfo ci = {};
        ci.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ci.stage                           = shader.m_stage;
        ci.pName                           = "main";
        ci.module                          = shader.GetShaderModule();

        stagesCI.push_back(ci);
    }
    // ##################### VERTEX INPUT #####################
    auto bindingDescription = GetVertexBindingDescription();
    auto attribDescriptions = GetVertexAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInput = {};  // vertex info hardcoded for the moment
    vertexInput.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount        = 1;
    vertexInput.pVertexBindingDescriptions           = &bindingDescription;
    vertexInput.vertexAttributeDescriptionCount      = static_cast<uint32_t>(attribDescriptions.size());
    vertexInput.pVertexAttributeDescriptions         = attribDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo assembly = {};
    assembly.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // ##################### VIEWPORT #####################
    VkViewport viewport = {};
    viewport.width      = (float)createInfo.viewportExtent.width;
    viewport.height     = -(float)createInfo.viewportExtent.height;
    viewport.x          = 0.f;
    viewport.y          = (float)createInfo.viewportExtent.height;
    viewport.minDepth   = 0.0f;
    viewport.maxDepth   = 1.0f;

    VkRect2D scissor = {};
    scissor.offset   = {0, 0};
    scissor.extent   = createInfo.viewportExtent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pViewports                        = &viewport;
    viewportState.viewportCount                     = 1;
    viewportState.pScissors                         = &scissor;
    viewportState.scissorCount                      = 1;

    // ##################### RASTERIZATION #####################
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.cullMode                               = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace                              = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterizer.depthClampEnable                       = createInfo.depthClampEnable;
    rasterizer.rasterizerDiscardEnable                = false;
    rasterizer.lineWidth                              = 1.0f;
    rasterizer.depthBiasEnable                        = false;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType                                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.sampleShadingEnable                  = VK_TRUE;
    multisample.minSampleShading                     = 0.2f;  // closer to 1 is smoother
    multisample.rasterizationSamples                 = createInfo.msaaSamples;


    // ##################### COLOR BLEND #####################
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask                      = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
    colorBlendAttachment.blendEnable                         = true;
    colorBlendAttachment.srcColorBlendFactor                 = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor                 = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp                        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor                 = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor                 = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp                        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType                               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable                       = false;
    colorBlend.attachmentCount                     = 1;
    colorBlend.pAttachments                        = &colorBlendAttachment;


    // ##################### DEPTH #####################
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable                       = createInfo.useDepth;
    depthStencil.depthWriteEnable                      = createInfo.depthWriteEnable;
    depthStencil.depthCompareOp                        = createInfo.depthCompareOp;  // not OP_LESS because we have a depth prepass
    // depthStencil.depthBoundsTestEnable	= VK_TRUE;
    // depthStencil.minDepthBounds			= 0.0f;
    // depthStencil.maxDepthBounds			= 1.0f;
    depthStencil.stencilTestEnable                     = createInfo.useStencil;


    // ##################### LAYOUT #####################
    VkDescriptorSetLayout descSetLayout         = VulkanContext::GetGlobalDescSetLayout();
    VkPushConstantRange pcRange                 = VulkanContext::GetGlobalPushConstantRange();
    VkPipelineLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.setLayoutCount             = 1;
    layoutCreateInfo.pSetLayouts                = &descSetLayout;
    layoutCreateInfo.pushConstantRangeCount     = 1;
    layoutCreateInfo.pPushConstantRanges        = &pcRange;

    VK_CHECK(vkCreatePipelineLayout(VulkanContext::GetDevice(), &layoutCreateInfo, nullptr, &m_layout), "Failed to create pipeline layout");


    // ##################### RENDERING #####################
    VkPipelineRenderingCreateInfo renderingCreateInfo = {};
    renderingCreateInfo.sType                         = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCreateInfo.viewMask                      = createInfo.viewMask;
    VkFormat defaultColorFormat                       = VulkanContext::GetSwapchainImageFormat();
    if(createInfo.colorFormats.empty() && createInfo.useColor)
    {
        renderingCreateInfo.colorAttachmentCount    = 1;
        renderingCreateInfo.pColorAttachmentFormats = &defaultColorFormat;
    }
    else
    {
        renderingCreateInfo.colorAttachmentCount    = createInfo.colorFormats.size();
        renderingCreateInfo.pColorAttachmentFormats = createInfo.colorFormats.data();
    }

    renderingCreateInfo.depthAttachmentFormat   = createInfo.useDepth ? createInfo.depthFormat : VK_FORMAT_UNDEFINED;
    renderingCreateInfo.stencilAttachmentFormat = createInfo.useStencil ? createInfo.stencilFormat : VK_FORMAT_UNDEFINED;

    // ##################### PIPELINE #####################
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount                   = static_cast<uint32_t>(stagesCI.size());
    pipelineInfo.pStages                      = stagesCI.data();
    pipelineInfo.pVertexInputState            = &vertexInput;
    pipelineInfo.pInputAssemblyState          = &assembly;
    pipelineInfo.pViewportState               = &viewportState;
    pipelineInfo.pRasterizationState          = &rasterizer;
    if(createInfo.useMultiSampling)
        pipelineInfo.pMultisampleState = &multisample;
    if(createInfo.useColorBlend)
        pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDepthStencilState = &depthStencil;
    if(createInfo.allowDerivatives)
        pipelineInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    else if(createInfo.parent)
    {
        pipelineInfo.flags              = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        pipelineInfo.basePipelineHandle = createInfo.parent->m_pipeline;
        pipelineInfo.basePipelineIndex  = -1;
    }
    // pipelineInfo.pDynamicState = &dynamicState;

    pipelineInfo.layout     = m_layout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass    = 0;

    pipelineInfo.pNext = &renderingCreateInfo;

    VK_CHECK(vkCreateGraphicsPipelines(VulkanContext::GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline), "Failed to create graphics pipeline");
}

void Pipeline::CreateComputePipeline(const PipelineCreateInfo& createInfo)
{
    VkPipelineShaderStageCreateInfo shaderCi = {};
    shaderCi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderCi.stage                           = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderCi.pName                           = "main";
    shaderCi.module                          = m_shaders[0].GetShaderModule();  // only 1 compute shader allowed

    VkDescriptorSetLayout descSetLayout         = VulkanContext::GetGlobalDescSetLayout();
    VkPushConstantRange pcRange                 = VulkanContext::GetGlobalPushConstantRange();
    VkPipelineLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.setLayoutCount             = 1;
    layoutCreateInfo.pSetLayouts                = &descSetLayout;
    layoutCreateInfo.pushConstantRangeCount     = 1;
    layoutCreateInfo.pPushConstantRanges        = &pcRange;


    VK_CHECK(vkCreatePipelineLayout(VulkanContext::GetDevice(), &layoutCreateInfo, nullptr, &m_layout), "Failed to create compute pipeline layout");


    VkComputePipelineCreateInfo pipelineCI = {};
    pipelineCI.sType                       = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCI.layout                      = m_layout;
    pipelineCI.stage                       = shaderCi;
    if(createInfo.allowDerivatives)
        pipelineCI.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    else if(createInfo.parent)
    {
        pipelineCI.flags              = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        pipelineCI.basePipelineHandle = createInfo.parent->m_pipeline;
        pipelineCI.basePipelineIndex  = -1;
    }

    VK_CHECK(vkCreateComputePipelines(VulkanContext::GetDevice(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_pipeline), "Failed to create compute pipeline");
}
