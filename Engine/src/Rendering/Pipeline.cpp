#include "Pipeline.hpp"
#include "Application.hpp"
#include "Core/Events/EventHandler.hpp"
#include "Rendering/VulkanContext.hpp"
#include "Rendering/MaterialSystem.hpp"

Pipeline::Pipeline(const std::string& shaderName, PipelineCreateInfo createInfo, uint16_t priority)
    : m_renderer(Application::GetInstance()->GetRenderer()),
      m_name(shaderName),
      m_createInfo(createInfo),
      m_priority(priority),
      m_shaderDataSlots({})
{
    Setup();

    Application::GetInstance()->GetEventHandler()->Subscribe(this, &Pipeline::OnPipelineReload);

    m_renderer->AddShaderButton(m_name);
}

void Pipeline::Setup()
{
    // m_shaderDataSlots.clear();
    m_vertexInputAttributes.clear();

    if(m_createInfo.type == PipelineType::GRAPHICS)
    {
        if(m_createInfo.stages & VK_SHADER_STAGE_VERTEX_BIT)
        {
            m_shaders.emplace_back(m_name, VK_SHADER_STAGE_VERTEX_BIT, this);
        }
        if(m_createInfo.stages & VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            m_shaders.emplace_back(m_name, VK_SHADER_STAGE_FRAGMENT_BIT, this);
        }
    }
    else
    {
        m_shaders.emplace_back(m_name, VK_SHADER_STAGE_COMPUTE_BIT, this);
    }


    if(m_createInfo.type == PipelineType::GRAPHICS)
        CreateGraphicsPipeline();
    else
        CreateComputePipeline();

    for(auto& shader : m_shaders)
    {
        shader.DestroyShaderModule();
    }
    m_shaders.clear();

    VK_SET_DEBUG_NAME(m_pipeline, VK_OBJECT_TYPE_PIPELINE, m_name.c_str());
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


void Pipeline::OnPipelineReload(PipelineReloadEvent e)
{
    if(m_name == e.name)
    {
        LOG_INFO("Reloading pipeline: {}", m_name);

        vkDeviceWaitIdle(VulkanContext::GetDevice());
        vkDestroyPipeline(VulkanContext::GetDevice(), m_pipeline, nullptr);
        vkDestroyPipelineLayout(VulkanContext::GetDevice(), m_layout, nullptr);
        m_pipeline = VK_NULL_HANDLE;
        m_layout   = VK_NULL_HANDLE;

        Setup();
    }
}


uint64_t Pipeline::GetMaterialBufferPtr() const
{
    return Application::GetInstance()->GetMaterialSystem()->GetMaterialBufferAddress(m_name);
}


void Pipeline::CreateGraphicsPipeline()
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

    VkPipelineVertexInputStateCreateInfo vertexInput = {};  // vertex info hardcoded for the moment
    vertexInput.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if(m_vertexInputBinding.has_value())
    {
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions    = &m_vertexInputBinding.value();
        ;
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttributes.size());
        vertexInput.pVertexAttributeDescriptions    = m_vertexInputAttributes.data();
    }

    VkPipelineInputAssemblyStateCreateInfo assembly = {};
    assembly.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // ##################### VIEWPORT #####################

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount                     = 1;
    viewportState.scissorCount                      = 1;
    VkViewport viewport                             = {};
    VkRect2D scissor                                = {};
    if(!m_createInfo.useDynamicViewport)
    {
        viewport.width    = (float)m_createInfo.viewportExtent.width;
        viewport.height   = -(float)m_createInfo.viewportExtent.height;
        viewport.x        = 0.f;
        viewport.y        = (float)m_createInfo.viewportExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;


        scissor.offset = {0, 0};
        scissor.extent = m_createInfo.viewportExtent;


        viewportState.pViewports = &viewport;
        viewportState.pScissors  = &scissor;
    }


    // ##################### DYNAMIC VIEWPORT #####################
    std::vector<VkDynamicState> dynamicStates;
    if(m_createInfo.useDynamicViewport)
    {
        dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
        dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
    }
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType                            = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount                = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates                   = dynamicStates.data();


    // ##################### RASTERIZATION #####################
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.cullMode                               = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace                              = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterizer.depthClampEnable                       = m_createInfo.depthClampEnable;
    rasterizer.rasterizerDiscardEnable                = false;
    rasterizer.lineWidth                              = 1.0f;
    rasterizer.depthBiasEnable                        = false;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType                                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.sampleShadingEnable                  = VK_TRUE;
    multisample.minSampleShading                     = 0.2f;  // closer to 1 is smoother
    multisample.rasterizationSamples                 = m_createInfo.useMultiSampling ? m_createInfo.msaaSamples : VK_SAMPLE_COUNT_1_BIT;


    // ##################### COLOR BLEND #####################
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask                      = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
    colorBlendAttachment.blendEnable                         = m_createInfo.useColorBlend;
    colorBlendAttachment.srcColorBlendFactor                 = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor                 = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp                        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor                 = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor                 = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp                        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType                               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable                       = VK_FALSE;
    colorBlend.attachmentCount                     = 1;
    colorBlend.pAttachments                        = &colorBlendAttachment;


    // ##################### DEPTH #####################
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable                       = m_createInfo.useDepth;
    depthStencil.depthWriteEnable                      = m_createInfo.depthWriteEnable;
    depthStencil.depthCompareOp                        = m_createInfo.depthCompareOp;  // not OP_LESS because we have a depth prepass
    // depthStencil.depthBoundsTestEnable	= VK_TRUE;
    // depthStencil.minDepthBounds			= 0.0f;
    // depthStencil.maxDepthBounds			= 1.0f;
    depthStencil.stencilTestEnable                     = m_createInfo.useStencil;


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
    renderingCreateInfo.viewMask                      = m_createInfo.viewMask;
    VkFormat defaultColorFormat                       = VulkanContext::GetSwapchainImageFormat();
    if(m_createInfo.colorFormats.empty() && m_createInfo.useColor)
    {
        renderingCreateInfo.colorAttachmentCount    = 1;
        renderingCreateInfo.pColorAttachmentFormats = &defaultColorFormat;
    }
    else
    {
        renderingCreateInfo.colorAttachmentCount    = static_cast<uint32_t>(m_createInfo.colorFormats.size());
        renderingCreateInfo.pColorAttachmentFormats = m_createInfo.colorFormats.data();
    }

    renderingCreateInfo.depthAttachmentFormat   = m_createInfo.useDepth ? m_createInfo.depthFormat : VK_FORMAT_UNDEFINED;
    renderingCreateInfo.stencilAttachmentFormat = m_createInfo.useStencil ? m_createInfo.stencilFormat : VK_FORMAT_UNDEFINED;

    // ##################### PIPELINE #####################
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount                   = static_cast<uint32_t>(stagesCI.size());
    pipelineInfo.pStages                      = stagesCI.data();
    pipelineInfo.pVertexInputState            = &vertexInput;
    pipelineInfo.pInputAssemblyState          = &assembly;
    pipelineInfo.pViewportState               = &viewportState;
    pipelineInfo.pRasterizationState          = &rasterizer;
    pipelineInfo.pMultisampleState            = &multisample;
    pipelineInfo.pColorBlendState             = &colorBlend;
    pipelineInfo.pDepthStencilState           = &depthStencil;
    if(m_createInfo.allowDerivatives)
        pipelineInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    else if(m_createInfo.parent)
    {
        pipelineInfo.flags              = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        pipelineInfo.basePipelineHandle = m_createInfo.parent->m_pipeline;
        pipelineInfo.basePipelineIndex  = -1;
    }
    if(m_createInfo.useDynamicViewport)
        pipelineInfo.pDynamicState = &dynamicState;

    pipelineInfo.layout     = m_layout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass    = 0;

    pipelineInfo.pNext = &renderingCreateInfo;

    VK_CHECK(vkCreateGraphicsPipelines(VulkanContext::GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline), "Failed to create graphics pipeline");
}

void Pipeline::CreateComputePipeline()
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
    if(m_createInfo.allowDerivatives)
        pipelineCI.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    else if(m_createInfo.parent)
    {
        pipelineCI.flags              = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        pipelineCI.basePipelineHandle = m_createInfo.parent->m_pipeline;
        pipelineCI.basePipelineIndex  = -1;
    }

    VK_CHECK(vkCreateComputePipelines(VulkanContext::GetDevice(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_pipeline), "Failed to create compute pipeline");
}

void Pipeline::Bind(CommandBuffer& cb) const
{
    switch(m_createInfo.type)
    {
    case PipelineType::GRAPHICS:
        vkCmdBindPipeline(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        break;
    case PipelineType::COMPUTE:
        vkCmdBindPipeline(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
        break;
    default:
        LOG_ERROR("Invalid pipeline type");
        break;
    }

    if(m_usesDescriptorSet)
    {
        cb.BindGlobalDescSet(GetBindPoint(), m_layout);
    }
}
void Pipeline::SetPushConstants(CommandBuffer& cb, void* data, uint32_t size, uint32_t offset)
{
    if(offset + size > 128)
    {
        LOG_ERROR("Push constant size is too big");
        return;
    }
    vkCmdPushConstants(cb.GetCommandBuffer(), m_layout, VK_SHADER_STAGE_ALL, offset, size, data);
}
