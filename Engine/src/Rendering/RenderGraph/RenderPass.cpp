#include "RenderPass.hpp"
#include "RenderGraph.hpp"
#include "vulkan/vulkan.h"
#include "RenderingResource.hpp"
#include "vulkan/vulkan_core.h"

RenderingTextureResource& RenderPass::AddTextureInput(const std::string& name, bool external)
{
    auto& resource = m_graph.GetTextureResource(name);

    if(external)
        resource.SetLifetime(RenderingResource::Lifetime::External);

    // If the format is not undefined then the resource has already been specified before
    // for example as a color output or depth output in which case we keep that texture info
    // and we only add to its usage flags
    if(resource.GetTextureInfo().format == VK_FORMAT_UNDEFINED)
    {
        TextureInfo info = {};
        info.format      = VK_FORMAT_UNDEFINED;
        info.usageFlags  = VK_IMAGE_USAGE_SAMPLED_BIT;
        resource.SetTextureInfo(info);
    }
    else
    {
        TextureInfo info  = resource.GetTextureInfo();
        info.usageFlags  |= VK_IMAGE_USAGE_SAMPLED_BIT;
        resource.SetTextureInfo(info);
    }
    resource.AddQueueUse(m_type);
    // TODO : Add proper pipeline stage
    VkPipelineStageFlags2 stage = 0;
    if(m_type == QueueTypeFlagBits::Graphics)
        stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    else if(m_type == QueueTypeFlagBits::Compute)
        stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    resource.AddUse(m_id, stage, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

    m_graph.RegisterResourceRead(name, *this);
    m_textureInputs.push_back(&resource);
    return resource;
}


RenderingTextureResource& RenderPass::AddColorOutput(const std::string& name, AttachmentInfo attachmentInfo, const std::string& input)
{
    auto& resource    = m_graph.GetTextureResource(name);
    TextureInfo info  = resource.GetTextureInfo();
    info.format       = VulkanContext::GetSwapchainImageFormat();
    info.usageFlags  |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if(name == SWAPCHAIN_RESOURCE_NAME)
    {
        info.usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    info.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    info.layout      = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

    if(attachmentInfo.clear)
    {
        info.SetClearValue(attachmentInfo.clearValue);
    }
    info.sizeModifier = attachmentInfo.sizeModifier;
    info.width        = attachmentInfo.width;
    info.height       = attachmentInfo.height;

    resource.SetTextureInfo(info);
    resource.AddUse(m_id, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    m_graph.RegisterResourceWrite(name, *this);
    m_colorOutputs.push_back(&resource);

    if(input != "")
    {
        auto& inputResource = m_graph.GetTextureResource(input);
        inputResource.AddQueueUse(m_type);
        inputResource.AddUse(m_id, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);
        m_graph.RegisterResourceRead(input, *this);
        m_colorInputs.push_back(&inputResource);
    }
    else
    {
        m_colorInputs.push_back(nullptr);
    }

    return resource;
}

RenderingTextureResource& RenderPass::AddDepthInput(const std::string& name)
{
    auto& resource = m_graph.GetTextureResource(name);

    TextureInfo info  = resource.GetTextureInfo();
    info.format       = VK_FORMAT_D32_SFLOAT;
    info.usageFlags  |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    info.aspectFlags  = VK_IMAGE_ASPECT_DEPTH_BIT;
    info.layout       = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    VkClearValue clear{};
    clear.depthStencil.depth = 0.0f;
    info.SetClearValue(clear);

    resource.SetTextureInfo(info);
    resource.AddQueueUse(m_type);
    resource.AddUse(m_id, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
    m_graph.RegisterResourceRead(name, *this);
    m_depthInput = &resource;
    return resource;
}

RenderingTextureResource& RenderPass::AddDepthOutput(const std::string& name, AttachmentInfo attachmentInfo)
{
    auto& resource = m_graph.GetTextureResource(name);

    TextureInfo info  = resource.GetTextureInfo();
    info.format       = VK_FORMAT_D32_SFLOAT;
    info.usageFlags  |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    info.aspectFlags  = VK_IMAGE_ASPECT_DEPTH_BIT;
    info.layout       = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;


    if(attachmentInfo.clear)
    {
        info.SetClearValue(attachmentInfo.clearValue);
    }
    info.sizeModifier = attachmentInfo.sizeModifier;
    info.width        = attachmentInfo.width;
    info.height       = attachmentInfo.height;

    resource.SetTextureInfo(info);
    resource.AddQueueUse(m_type);
    resource.AddUse(m_id, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    m_graph.RegisterResourceWrite(name, *this);
    m_depthOutput = &resource;
    return resource;
}

RenderingTextureResource& RenderPass::AddResolveOutput(const std::string& name)
{
    auto& resource = m_graph.GetTextureResource(name);

    TextureInfo info = {};
    info.format      = VK_FORMAT_R8G8B8A8_UNORM;
    info.usageFlags  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if(name == SWAPCHAIN_RESOURCE_NAME)
    {
        info.usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    info.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    info.layout      = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

    resource.SetTextureInfo(info);
    resource.AddUse(m_id, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    m_graph.RegisterResourceWrite(name, *this);
    m_resolveOutputs.push_back(&resource);
    return resource;
}


RenderingBufferResource& RenderPass::AddBufferInput(const std::string& name, VkDeviceSize size, VkPipelineStageFlags2 stageFlags, VkBufferUsageFlags usageFlags)
{
    auto& resource = m_graph.GetBufferResource(name);
    resource.SetBufferInfo({size, stageFlags, usageFlags});
    resource.AddQueueUse(m_type);

    m_graph.RegisterResourceRead(name, *this);
    return resource;
}

RenderingBufferResource& RenderPass::AddDrawCommandBuffer(const std::string& name)
{
    auto& resource = AddBufferInput(name, sizeof(DrawCommand) * MAX_DRAW_COMMANDS, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    resource.AddUse(m_id, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
    // TODO lifetime? should be permanent i think but i still want the renderpass to allocate it which isnt possible yet (but also transient resources are kind of permanent for now as they aren't reset on each frame)

    m_drawCommandBuffers.push_back(&resource);

    return resource;
}
RenderingBufferResource& RenderPass::AddVertexBufferInput(const std::string& name)
{
    auto& resource = AddBufferInput(name, 1024, VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    resource.SetLifetime(RenderingResource::Lifetime::External);
    resource.AddUse(m_id, VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT);

    m_vertexBufferInputs.push_back(&resource);

    return resource;
}

RenderingBufferResource& RenderPass::AddIndexBufferInput(const std::string& name)
{
    auto& resource = AddBufferInput(name, 1024, VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    resource.SetLifetime(RenderingResource::Lifetime::External);
    resource.AddUse(m_id, VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT);

    m_indexBufferInputs.push_back(&resource);
    return resource;
}

RenderingBufferResource& RenderPass::AddUniformBufferInput(const std::string& name, VkPipelineStageFlags2 stages, bool external)
{
    if(stages == 0)
    {
        if(m_type == QueueTypeFlagBits::Graphics)
            stages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        else if(m_type == QueueTypeFlagBits::Compute)
            stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    auto& resource = AddBufferInput(name, 1024, stages, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    resource.AddUse(m_id, stages, VK_ACCESS_2_UNIFORM_READ_BIT);
    if(external)
        resource.SetLifetime(RenderingResource::Lifetime::External);

    m_uniformBufferInputs.push_back(&resource);
    return resource;
}

// TODO actually implement these correctly
RenderingBufferResource& RenderPass::AddStorageBufferReadOnly(const std::string& name, VkPipelineStageFlags2 stages, bool external)
{
    if(stages == 0)
    {
        if(m_type == QueueTypeFlagBits::Graphics)
            stages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        else if(m_type == QueueTypeFlagBits::Compute)
            stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    auto& resource = AddBufferInput(name, 1024, stages, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    if(external)
        resource.SetLifetime(RenderingResource::Lifetime::External);

    resource.AddUse(m_id, stages, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

    m_storageBufferInputs.push_back(&resource);
    m_storageBufferOutputs.push_back(nullptr);
    return resource;
}

RenderingBufferResource& RenderPass::AddStorageBufferOutput(const std::string& name, const std::string& input, VkPipelineStageFlags2 stages, bool external)
{
    if(stages == 0)
    {
        if(m_type == QueueTypeFlagBits::Graphics)
            stages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        else if(m_type == QueueTypeFlagBits::Compute)
            stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    auto& resource = m_graph.GetBufferResource(name);
    resource.SetBufferInfo({32964000, stages, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT});
    resource.AddQueueUse(m_type);
    resource.AddUse(m_id, stages, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

    m_graph.RegisterResourceWrite(name, *this);
    if(external)
        resource.SetLifetime(RenderingResource::Lifetime::External);

    m_storageBufferOutputs.push_back(&resource);
    if(input != "")
    {
        auto& inputResource = m_graph.GetBufferResource(input);
        inputResource.AddQueueUse(m_type);
        inputResource.AddUse(m_id, stages, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
        if(external)
            inputResource.SetLifetime(RenderingResource::Lifetime::External);
        m_graph.RegisterResourceRead(input, *this);
        m_storageBufferInputs.push_back(&inputResource);
    }
    else
    {
        m_storageBufferInputs.push_back(nullptr);
    }

    return resource;
}

RenderingTextureArrayResource& RenderPass::AddTextureArrayInput(const std::string& name, VkPipelineStageFlags2 stages, VkAccessFlags2 access)
{
    auto& resource = m_graph.GetTextureArrayResource(name);
    m_graph.RegisterResourceRead(name, *this);
    resource.AddQueueUse(m_type);
    resource.AddUse(m_id, stages, access);
    m_textureArrayInputs.push_back(&resource);
    return resource;
}
RenderingTextureArrayResource& RenderPass::AddTextureArrayOutput(const std::string& name, VkPipelineStageFlags2 stages, VkAccessFlags2 access)
{
    auto& resource = m_graph.GetTextureArrayResource(name);
    m_graph.RegisterResourceWrite(name, *this);
    resource.AddQueueUse(m_type);
    resource.AddUse(m_id, stages, access);
    m_textureArrayOutputs.push_back(&resource);
    return resource;
}
