#include "RenderGraph.hpp"
#include "Rendering/Image.hpp"
#include "Rendering/RenderGraph/RenderPass.hpp"
#include "Rendering/RenderGraph/RenderingResource.hpp"
#include "Rendering/VulkanContext.hpp"
#include "vulkan/vulkan.h"
#include "vulkan/vulkan_core.h"
#include <stack>
#include <stdint.h>

void RenderGraph::SetupSwapchainImages(const std::vector<VkImage>& swapchainImages)
{
    m_swapchainImages.clear();
    m_swapchainImages.reserve(swapchainImages.size());
    for(const auto& img : swapchainImages)
    {
        ImageCreateInfo ci = {};
        ci.image           = img;
        ci.aspectFlags     = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.format          = VulkanContext::GetSwapchainImageFormat();

        m_swapchainImages.emplace_back(VulkanContext::GetSwapchainExtent(), ci);
    }
}

RenderPass2& RenderGraph::AddRenderPass(const std::string& name, QueueTypeFlagBits type)
{
    auto it = m_renderPassIds.find(name);
    if(it != m_renderPassIds.end())
        return *m_renderPasses[it->second];

    uint32_t id = m_renderPasses.size();
    m_renderPasses.emplace_back(new RenderPass2(*this, id, type));
    m_renderPasses.back()->SetName(name);
    m_renderPassIds[name] = id;
    return *m_renderPasses.back();
}


RenderingTextureResource& RenderGraph::GetTextureResource(const std::string& name)
{
    /*
    if(name == SWAPCHAIN_RESOURCE_NAME)
        return m_swapchainResource;
    */
    auto it = m_resourceIds.find(name);
    if(it != m_resourceIds.end())
        return static_cast<RenderingTextureResource&>(*m_ressources[it->second]);

    m_resourceIds[name] = m_ressources.size();
    m_ressources.push_back(std::make_unique<RenderingTextureResource>(name));
    return static_cast<RenderingTextureResource&>(*m_ressources.back());
}

RenderingBufferResource& RenderGraph::GetBufferResource(const std::string& name)
{
    auto it = m_resourceIds.find(name);
    if(it != m_resourceIds.end())
        return static_cast<RenderingBufferResource&>(*m_ressources[it->second]);

    m_resourceIds[name] = m_ressources.size();
    m_ressources.push_back(std::make_unique<RenderingBufferResource>(name));
    return static_cast<RenderingBufferResource&>(*m_ressources.back());
}

void RenderGraph::RegisterResourceRead(const std::string& name, const RenderPass2& renderPass)
{
    m_resourceReads[name].push_back(renderPass.GetId());
}

void RenderGraph::RegisterResourceWrite(const std::string& name, const RenderPass2& renderPass)
{
    m_resourceWrites[name].push_back(renderPass.GetId());
}

void RenderGraph::Build()
{
    CreateEdges();
    OrderPasses();
    CreatePhysicalResources();
    CreatePhysicalPasses();
    InitialisePasses();
    AddSynchronization();
    ToDOT("graph.dot");

    m_isBuilt = true;
}


void RenderGraph::CreateEdges()
{
    m_graph.clear();
    m_graph.resize(m_renderPasses.size());
    for(auto& [name, writes] : m_resourceWrites)
    {
        auto it = m_resourceReads.find(name);
        if(it == m_resourceReads.end())
            continue;
        for(auto& writeNode : writes)
        {
            m_graph[writeNode].insert(it->second.begin(), it->second.end());
        }
    }
}

void RenderGraph::TopologicalSortUtil(uint32_t currentNode, std::stack<uint32_t>& stack, std::unordered_set<uint32_t>& visited)
{
    visited.insert(currentNode);
    for(const auto& node : m_graph[currentNode])
    {
        if(visited.find(node) == visited.end())
        {
            TopologicalSortUtil(node, stack, visited);
        }
    }
    stack.push(currentNode);
}
void RenderGraph::OrderPasses()
{
    std::unordered_set<uint32_t> visited;
    std::stack<uint32_t> stack;
    for(uint32_t node = 0; node < m_graph.size(); ++node)
    {
        if(visited.find(node) == visited.end())
        {
            TopologicalSortUtil(node, stack, visited);
        }
    }

    m_orderedPasses.reserve(m_renderPasses.size());

    while(!stack.empty())
    {
        m_orderedPasses.push_back(m_renderPasses[stack.top()]);
        stack.pop();
    }
}

void RenderGraph::FindResourceLifetimes()
{
    for(uint32_t i = 0; i < m_orderedPasses.size(); ++i)
    {
        auto* pass = m_orderedPasses[i];
        for(auto* resource : pass->GetColorInputs())
            resource->AddAccess(i);
        for(auto* resource : pass->GetColorOutputs())
            resource->AddAccess(i);

        pass->GetDepthInput()->AddAccess(i);
        pass->GetDepthOutput()->AddAccess(i);

        for(auto* resource : pass->GetResolveOutputs())
            resource->AddAccess(i);
        pass->GetDepthResolveOutput()->AddAccess(i);

        for(auto* resource : pass->GetVertexBufferInputs())
            resource->AddAccess(i);
        for(auto* resource : pass->GetIndexBufferInputs())
            resource->AddAccess(i);
        for(auto* resource : pass->GetUniformBufferInputs())
            resource->AddAccess(i);
        for(auto* resource : pass->GetTextureInputs())
            resource->AddAccess(i);
        for(auto* resource : pass->GetStorageBufferInputs())
            resource->AddAccess(i);
        for(auto* resource : pass->GetStorageBufferOutputs())
            resource->AddAccess(i);
    }
}

void RenderGraph::CreatePhysicalResources()
{
    struct ImageInfo
    {
        int32_t width  = -1;
        int32_t height = -1;
        ImageCreateInfo createInfo;
    };
    std::vector<ImageInfo> imageInfos;
    struct BufferCreateInfo
    {
        uint64_t size;
        VkBufferUsageFlags usage;
        VkMemoryPropertyFlags memoryFlags;
    };
    std::vector<BufferCreateInfo> bufferInfos;

    auto AddOrUpdateImageInfo = [&](RenderingTextureResource* resource)
    {
        if(resource->GetPhysicalId() == -1)
        {
            uint32_t id = imageInfos.size();
            resource->SetPhysicalId(id);

            auto inputTextureInfo = resource->GetTextureInfo();

            ImageInfo info{};
            info.createInfo.format      = inputTextureInfo.format;
            info.createInfo.usage       = inputTextureInfo.usageFlags;
            info.createInfo.aspectFlags = inputTextureInfo.aspectFlags;
            info.createInfo.msaaSamples = inputTextureInfo.samples;
            info.createInfo.useMips     = inputTextureInfo.mipLevels > 1;
            info.createInfo.layout      = inputTextureInfo.layout;

            switch(inputTextureInfo.sizeModifier)
            {
            case SizeModifier::Absolute:
                {
                    info.width  = inputTextureInfo.width;
                    info.height = inputTextureInfo.height;
                }
                break;
            case SizeModifier::SwapchainRelative:
                {
                    info.width  = VulkanContext::GetSwapchainExtent().width * inputTextureInfo.width;
                    info.height = VulkanContext::GetSwapchainExtent().height * inputTextureInfo.height;
                    ;
                }
            default:
                LOG_ERROR("Unsupported size modifier");
            }

            imageInfos.push_back(info);
        }
        else
        {
            imageInfos[resource->GetPhysicalId()].createInfo.usage |= resource->GetTextureInfo().usageFlags;
        }
    };
    auto AddOrUpdateBufferInfo = [&](RenderingBufferResource* resource)
    {
        if(resource->GetPhysicalId() == -1)
        {
            auto bufferInfo = resource->GetBufferInfo();
            uint32_t id     = m_transientBuffers.size();
            resource->SetPhysicalId(id);
            bufferInfos.push_back({bufferInfo.size, bufferInfo.usageFlags, bufferInfo.memoryFlags});
        }
        else
        {
            bufferInfos[resource->GetPhysicalId()].usage |= resource->GetBufferInfo().usageFlags;
            bufferInfos[resource->GetPhysicalId()].memoryFlags |= resource->GetBufferInfo().memoryFlags;
        }
    };

    // Trivial merge: merge inputs with their corresponding output
    for(auto* pass : m_orderedPasses)
    {
        auto colorInputs  = pass->GetColorInputs();
        auto colorOutputs = pass->GetColorOutputs();
        for(uint32_t i = 0; i < colorInputs.size(); ++i)
        {
            auto* input  = colorInputs[i];
            auto* output = colorOutputs[i];
            if(input)
            {
                AddOrUpdateImageInfo(input);

                // We can't have an input without an output for color attachments so no need to check for nullptr
                if(output->GetPhysicalId() == -1)
                    output->SetPhysicalId(input->GetPhysicalId());
                else if(output->GetPhysicalId() != input->GetPhysicalId())
                    LOG_ERROR("Can't alias resources. Output already has a different index");
            }
        }
        for(auto* output : colorOutputs)
        {
            AddOrUpdateImageInfo(output);
        }

        for(auto* output : pass->GetResolveOutputs())
        {
            AddOrUpdateImageInfo(output);
        }

        auto* depthInput  = pass->GetDepthInput();
        auto* depthOutput = pass->GetDepthOutput();
        if(depthInput)
        {
            AddOrUpdateImageInfo(depthInput);
            if(depthOutput)
            {
                if(depthOutput->GetPhysicalId() == -1)
                    depthOutput->SetPhysicalId(depthInput->GetPhysicalId());
                else if(depthOutput->GetPhysicalId() != depthInput->GetPhysicalId())
                    LOG_ERROR("Can't alias resources. Depth output already has a different index");
            }
        }

        if(depthOutput)
        {
            AddOrUpdateImageInfo(depthOutput);
        }

        auto* depthResolveOutput = pass->GetDepthResolveOutput();
        if(depthResolveOutput)
        {
            AddOrUpdateImageInfo(depthResolveOutput);
        }

        // Index and vertex buffers are always external resources so they are not created in the render graph

        for(auto* input : pass->GetUniformBufferInputs())
        {
            AddOrUpdateBufferInfo(input);
        }

        for(auto* input : pass->GetTextureInputs())
        {
            if(input->GetLifetime() == RenderingResource::Lifetime::External)
                continue;

            AddOrUpdateImageInfo(input);
        }

        auto storageBufferInputs  = pass->GetStorageBufferInputs();
        auto storageBufferOutputs = pass->GetStorageBufferOutputs();
        for(uint32_t i = 0; i < storageBufferInputs.size(); ++i)
        {
            auto* input  = storageBufferInputs[i];
            auto* output = storageBufferOutputs[i];
            if(input)
            {
                if(input->GetLifetime() == RenderingResource::Lifetime::External)
                    continue;

                AddOrUpdateBufferInfo(input);

                // if output is nullptr then its a read only buffer
                if(!output)
                    continue;
                if(output->GetPhysicalId() == -1)
                    output->SetPhysicalId(input->GetPhysicalId());
                else if(output->GetPhysicalId() != input->GetPhysicalId())
                    LOG_ERROR("Can't alias resources. Output already has a different index");
            }
        }
        for(auto* output : storageBufferOutputs)
        {
            if(!output)  // read only buffer so it was handled in the input loop
                continue;
            if(output->GetLifetime() == RenderingResource::Lifetime::External)
                continue;

            AddOrUpdateBufferInfo(output);
        }
    }

    for(auto& info : imageInfos)
    {
        m_transientImages.emplace_back(info.width, info.height, info.createInfo);
    }
    for(auto& info : bufferInfos)
    {
        m_transientBuffers.emplace_back(info.size, info.usage, info.memoryFlags);
    }
}

void RenderGraph::CreatePhysicalPasses()
{
    std::unordered_set<uint32_t> colorAttachmentIds;
    std::unordered_set<uint32_t> depthAttachmentIds;
    for(auto& pass : m_orderedPasses)
    {
        VkRenderingInfo rendering{};
        rendering.sType             = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering.flags             = 0;
        rendering.layerCount        = 1;
        rendering.renderArea.extent = VulkanContext::GetSwapchainExtent();

        auto colorOutputs   = pass->GetColorOutputs();
        auto* depthOutput   = pass->GetDepthOutput();
        auto* depthInput    = pass->GetDepthInput();
        auto resolveOutputs = pass->GetResolveOutputs();

        for(int i = 0; i < colorOutputs.size(); ++i)
        {
            VkRenderingAttachmentInfo info{};
            info.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            uint32_t id      = UINT32_MAX;  // use this to indicate swapchain image, get actual id if not swapchain image
            auto textureInfo = colorOutputs[i]->GetTextureInfo();
            bool isSwapchain = colorOutputs[i]->GetName() == SWAPCHAIN_RESOURCE_NAME;
            id               = colorOutputs[i]->GetPhysicalId();

            info.imageView = m_transientImages[id].GetImageView();

            info.imageLayout = textureInfo.layout;
            info.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
            if(isSwapchain)
            {
                info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                info.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
            }
            if(colorAttachmentIds.find(id) != colorAttachmentIds.end())
            {
                info.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            }
            else
            {
                colorAttachmentIds.insert(id);
                if(textureInfo.clear)
                {
                    info.loadOp     = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    info.clearValue = textureInfo.clearValue;
                }
                else
                    info.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            }

            if(!resolveOutputs.empty())
            {
                info.resolveMode        = VK_RESOLVE_MODE_AVERAGE_BIT;
                info.resolveImageLayout = resolveOutputs[i]->GetTextureInfo().layout;
                uint32_t resolveId      = resolveOutputs[i]->GetPhysicalId();
                info.resolveImageView   = m_transientImages[resolveId].GetImageView();
            }
            pass->AddAttachmentInfo(info, isSwapchain);
        }


        if(depthOutput)
        {
            uint32_t id      = depthOutput->GetPhysicalId();
            auto textureInfo = depthOutput->GetTextureInfo();
            VkRenderingAttachmentInfo info{};
            info.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            info.imageView   = m_transientImages[id].GetImageView();
            info.imageLayout = textureInfo.layout;
            info.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

            depthAttachmentIds.insert(id);
            if(textureInfo.clear)
            {
                info.loadOp     = VK_ATTACHMENT_LOAD_OP_CLEAR;
                info.clearValue = textureInfo.clearValue;
            }
            else
                info.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            auto* depthResolve = pass->GetDepthResolveOutput();
            if(depthResolve)
            {
                info.resolveMode        = VK_RESOLVE_MODE_AVERAGE_BIT;
                info.resolveImageLayout = depthResolve->GetTextureInfo().layout;
                uint32_t resolveId      = depthResolve->GetPhysicalId();
                info.resolveImageView   = m_transientImages[resolveId].GetImageView();
            }
            pass->AddAttachmentInfo(info);
            rendering.pDepthAttachment = &pass->GetAttachmentInfos().back();
        }
        else if(depthInput)
        {
            uint32_t id      = depthInput->GetPhysicalId();
            auto textureInfo = depthInput->GetTextureInfo();
            VkRenderingAttachmentInfo info{};
            info.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            info.imageView   = m_transientImages[id].GetImageView();
            info.imageLayout = textureInfo.layout;
            info.storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE;

            info.loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
            auto* depthResolve = pass->GetDepthResolveOutput();
            if(depthResolve)
            {
                info.resolveMode        = VK_RESOLVE_MODE_AVERAGE_BIT;
                info.resolveImageLayout = depthResolve->GetTextureInfo().layout;
                uint32_t resolveId      = depthResolve->GetPhysicalId();
                info.resolveImageView   = m_transientImages[resolveId].GetImageView();
            }
            pass->AddAttachmentInfo(info);
            rendering.pDepthAttachment = &pass->GetAttachmentInfos().back();
        }
        // TODO redo this probably, currently RenderPass::attachmentinofs contain all the attachments not just color
        // so assigning this way to pColorAttachments seems very error prone
        rendering.colorAttachmentCount = pass->GetColorOutputs().size();
        rendering.pColorAttachments    = pass->GetAttachmentInfos().data();
        pass->SetRenderingInfo(rendering);
    }
}


inline VkImageLayout ConvertAccessToLayout(VkAccessFlags2 access)
{
    if(access & (VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT))
        return VK_IMAGE_LAYOUT_GENERAL;

    // attachment with write access
    if(access & VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
        return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    // read only attachment
    if(access & (VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT))
        return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

    // sampled image
    if(access & VK_ACCESS_2_SHADER_SAMPLED_READ_BIT)
        return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;


    LOG_ERROR("Access flags not supported, using VK_IMAGE_LAYOUT_GENERAL");
    return VK_IMAGE_LAYOUT_GENERAL;
}

inline bool IsDepthStencilFormat(VkFormat format)
{
    return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_X8_D24_UNORM_PACK32 || format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_S8_UINT || format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

void RenderGraph::AddSynchronization()
{
    // just a helper function to avoid code duplication
    auto SetupImageSync = [&](uint32_t i, RenderingTextureResource* resource, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess)
    {
        // keep track of the stages we already synced this resource write to
        // in order to avoid redundant syncyng
        VkPipelineStageFlags2 alreadySyncedStages = 0;

        uint32_t srcPassId = m_orderedPasses[i]->GetId();
        for(uint32_t j = i + 1; j < m_orderedPasses.size(); ++j)
        {
            auto it = resource->GetUsages().find(m_orderedPasses[j]->GetId());
            if(it == resource->GetUsages().end())
                continue;

            auto [stages, usage] = it->second;
            if(stages & alreadySyncedStages)
                continue;

            uint32_t dstPassId = m_orderedPasses[j]->GetId();

            // if it's used in the next pass we should use a normal pipeline barrier because apparently
            // using a VkEvent isn't as good for performance as a normal barrier if we set it then immediately wait for it
            Image& image = m_transientImages[resource->GetPhysicalId()];
            VkImageMemoryBarrier2 barrier{};
            barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.srcStageMask        = srcStage;
            barrier.dstStageMask        = stages;
            barrier.srcAccessMask       = srcAccess;
            barrier.dstAccessMask       = usage;
            barrier.oldLayout           = ConvertAccessToLayout(srcAccess);
            barrier.newLayout           = ConvertAccessToLayout(usage);
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image               = image.GetImage();

            if(IsDepthStencilFormat(image.GetFormat()))
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            else
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
            if(j == i + 1)
            {
                m_imageBarriers[srcPassId].push_back(barrier);
            }
            else  // use an event so work between the two passes can be done without waiting for the synchronisation
            {
                // it is fine to do it like this instead of using find because if the event doesn't exist yet we would want to create it anyway, so the map inserting it when using the [] operator is fine because VulkanEvent's default constructor does the vkCreateEvent call
                auto& event = m_events[{srcPassId, dstPassId}];
                event.AddBarrier(barrier);
            }
            alreadySyncedStages |= stages;
        }
    };
    auto SetupBufferSync = [&](uint32_t i, RenderingBufferResource* resource, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess)
    {
        // keep track of the stages we already synced this resource write to
        // in order to avoid redundant syncyng
        VkPipelineStageFlags2 alreadySyncedStages = 0;

        uint32_t srcPassId = m_orderedPasses[i]->GetId();
        for(uint32_t j = i + 1; j < m_orderedPasses.size(); ++j)
        {
            auto it = resource->GetUsages().find(m_orderedPasses[j]->GetId());
            if(it == resource->GetUsages().end())
                continue;

            auto [stages, usage] = it->second;
            if(stages & alreadySyncedStages)
                continue;

            uint32_t dstPassId = m_orderedPasses[j]->GetId();

            // if it's used in the next pass we should use a normal pipeline barrier because apparently
            // using a VkEvent isn't as good for performance as a normal barrier if we set it then immediately wait for it
            VkBufferMemoryBarrier2 barrier{};
            barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barrier.srcStageMask        = srcStage;
            barrier.dstStageMask        = stages;
            barrier.srcAccessMask       = srcAccess;
            barrier.dstAccessMask       = usage;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer              = VK_NULL_HANDLE;  // m_transientBuffers[resource->GetPhysicalId()].GetVkBuffer();
            barrier.offset              = 0;
            barrier.size                = VK_WHOLE_SIZE;

            if(j == i + 1)
            {
                m_bufferBarriers[srcPassId].push_back(barrier);
            }
            else  // use an event so work between the two passes can be done without waiting for the synchronisation
            {
                // it is fine to do it like this instead of using find because if the event doesn't exist yet we would want to create it anyway, so the map inserting it when using the [] operator is fine because VulkanEvent's default constructor does the vkCreateEvent call
                auto& event = m_events[{srcPassId, dstPassId}];
                event.AddBarrier(barrier);
            }
            alreadySyncedStages |= stages;
        }
    };

    m_imageBarriers.resize(m_orderedPasses.size());
    m_bufferBarriers.resize(m_orderedPasses.size());

    for(uint32_t i = 0; i < m_orderedPasses.size(); ++i)
    {
        for(auto* resource : m_orderedPasses[i]->GetColorOutputs())
        {
            SetupImageSync(i, resource, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        }

        auto* depthOutput = m_orderedPasses[i]->GetDepthOutput();
        if(depthOutput)
        {
            SetupImageSync(i, depthOutput, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        }

        // TODO: do resolve stuff
        for(auto* resource : m_orderedPasses[i]->GetResolveOutputs())
        {
        }

        for(auto* resource : m_orderedPasses[i]->GetStorageBufferOutputs())
        {
            if(!resource)
                continue;

            auto [srcStage, _] = resource->GetUsages()[m_orderedPasses[i]->GetId()];

            SetupBufferSync(i, resource, srcStage, VK_ACCESS_2_SHADER_WRITE_BIT);
        }
    }


    // put the pointers in the correct places to facilitate finding the events to signal/wait on during the execute loop
    m_eventSignals.resize(m_orderedPasses.size());
    m_eventWaits.resize(m_orderedPasses.size());
    for(auto& [passes, event] : m_events)
    {
        const auto& [srcPassId, dstPassId] = passes;
        m_eventSignals[srcPassId].push_back(&event);
        m_eventWaits[dstPassId].push_back(&event);
    }
}

void RenderGraph::InitialisePasses()
{
    for(auto& pass : m_orderedPasses)
    {
        pass->Initialise();
    }
}

void RenderGraph::Execute(CommandBuffer& cb, uint32_t frameIndex)
{
    assert(m_isBuilt);
    // we don't allow reading from the swapchain image, I don't think it makes sense
    /*
    for(uint32_t passId : m_resourceWrites[SWAPCHAIN_RESOURCE_NAME])
    {
        m_renderPasses[passId]->SetSwapchainImageView(m_swapchainImages[frameIndex].GetImageView());
    }
    */

    auto& renderTarget = m_transientImages[m_ressources[m_resourceIds[SWAPCHAIN_RESOURCE_NAME]]->GetPhysicalId()];
    // transfer swapchain image to color attachment layout
    {
        VkImageMemoryBarrier swapchainBarrier{};
        swapchainBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        swapchainBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        swapchainBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        swapchainBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        swapchainBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        swapchainBarrier.image                           = m_swapchainImages[frameIndex].GetImage();
        swapchainBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        swapchainBarrier.subresourceRange.baseMipLevel   = 0;
        swapchainBarrier.subresourceRange.baseArrayLayer = 0;
        swapchainBarrier.subresourceRange.layerCount     = 1;
        swapchainBarrier.subresourceRange.levelCount     = 1;
        swapchainBarrier.srcAccessMask                   = 0;
        swapchainBarrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(cb.GetCommandBuffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapchainBarrier);
    }

    // transfer the render target back to color attachment layout
    {
        VkImageMemoryBarrier renderTargetBarrier{};
        renderTargetBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        renderTargetBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        renderTargetBarrier.newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        renderTargetBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        renderTargetBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        renderTargetBarrier.image                           = renderTarget.GetImage();
        renderTargetBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        renderTargetBarrier.subresourceRange.baseMipLevel   = 0;
        renderTargetBarrier.subresourceRange.baseArrayLayer = 0;
        renderTargetBarrier.subresourceRange.layerCount     = 1;
        renderTargetBarrier.subresourceRange.levelCount     = 1;
        renderTargetBarrier.srcAccessMask                   = 0;
        renderTargetBarrier.dstAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(cb.GetCommandBuffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &renderTargetBarrier);
    }

    for(auto& pass : m_orderedPasses)
    {
        if(pass->GetType() == QueueTypeFlagBits::Compute)
        {
            pass->Execute(cb, frameIndex);
        }
        else
        {
            vkCmdBeginRendering(cb.GetCommandBuffer(), pass->GetRenderingInfo());
            pass->Execute(cb, frameIndex);
            vkCmdEndRendering(cb.GetCommandBuffer());
        }
    }

    // Transfer the render target into transfer src optimal layout
    {
        VkImageMemoryBarrier renderTargetBarrier{};
        renderTargetBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        renderTargetBarrier.oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        renderTargetBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        renderTargetBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        renderTargetBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        renderTargetBarrier.image                           = renderTarget.GetImage();
        renderTargetBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        renderTargetBarrier.subresourceRange.baseMipLevel   = 0;
        renderTargetBarrier.subresourceRange.baseArrayLayer = 0;
        renderTargetBarrier.subresourceRange.layerCount     = 1;
        renderTargetBarrier.subresourceRange.levelCount     = 1;
        renderTargetBarrier.srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        renderTargetBarrier.dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cb.GetCommandBuffer(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &renderTargetBarrier);
    }

    VkImageBlit blitRegion{};
    blitRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.layerCount     = 1;
    blitRegion.srcSubresource.mipLevel       = 0;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcOffsets[0]                 = {0, 0, 0};
    blitRegion.srcOffsets[1].x               = renderTarget.GetWidth();
    blitRegion.srcOffsets[1].y               = renderTarget.GetHeight();
    blitRegion.srcOffsets[1].z               = 1;
    blitRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.layerCount     = 1;
    blitRegion.dstSubresource.mipLevel       = 0;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstOffsets[0]                 = {0, 0, 0};
    blitRegion.dstOffsets[1].x               = VulkanContext::GetSwapchainExtent().width;
    blitRegion.dstOffsets[1].y               = VulkanContext::GetSwapchainExtent().height;
    blitRegion.dstOffsets[1].z               = 1;


    vkCmdBlitImage(cb.GetCommandBuffer(), renderTarget.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_swapchainImages[frameIndex].GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_LINEAR);

    // transition swapchain image to present leayout
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = m_swapchainImages[frameIndex].GetImage();
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.subresourceRange.levelCount     = 1;
        barrier.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                   = VK_ACCESS_MEMORY_READ_BIT;
        vkCmdPipelineBarrier(cb.GetCommandBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
}

void RenderGraph::ToDOT(const std::string& filename)
{
    std::ofstream file(filename);
    file << "digraph G {\n";
    for(uint32_t node = 0; node < m_graph.size(); ++node)
    {
        std::cout << m_renderPasses[node]->GetName() << std::endl;
        for(const auto& edge : m_graph[node])
        {
            file << m_renderPasses[node]->GetName() << " -> " << m_renderPasses[edge]->GetName();
            if(m_events.find({node, edge}) != m_events.end())
            {
                file << " [label=\"Event\"];\n";
            }
            else if(!m_imageBarriers[node].empty())
            {
                file << " [label=\"ImageBarrier\"];\n";
            }
            else if(!m_bufferBarriers[node].empty())
            {
                file << " [label=\"BufferBarrier\"];\n";
            }
            else
                file << ";\n";
        }
    }
    file << "}";
    file.close();
}
