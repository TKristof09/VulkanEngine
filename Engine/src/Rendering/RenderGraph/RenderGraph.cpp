#include "RenderGraph.hpp"
#include "Rendering/Image.hpp"
#include "Rendering/RenderGraph/RenderPass.hpp"
#include "Rendering/RenderGraph/RenderingResource.hpp"
#include "Rendering/VulkanContext.hpp"
#include "vulkan/vulkan_core.h"
#include <vulkan/vulkan.h>
#include <stack>
#include <vulkan/vk_enum_string_helper.h>

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

RenderPass& RenderGraph::AddRenderPass(const std::string& name, QueueTypeFlagBits type)
{
    auto it = m_renderPassIds.find(name);
    if(it != m_renderPassIds.end())
        return *m_renderPasses[it->second];

    uint32_t id = m_renderPasses.size();
    m_renderPasses.emplace_back(new RenderPass(*this, id, type));
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
        return static_cast<RenderingTextureResource&>(*m_resources[it->second]);

    m_resourceIds[name] = m_resources.size();
    m_resources.push_back(std::make_unique<RenderingTextureResource>(name));
    return static_cast<RenderingTextureResource&>(*m_resources.back());
}

RenderingBufferResource& RenderGraph::GetBufferResource(const std::string& name)
{
    auto it = m_resourceIds.find(name);
    if(it != m_resourceIds.end())
        return static_cast<RenderingBufferResource&>(*m_resources[it->second]);

    m_resourceIds[name] = m_resources.size();
    m_resources.push_back(std::make_unique<RenderingBufferResource>(name));
    return static_cast<RenderingBufferResource&>(*m_resources.back());
}

void RenderGraph::RegisterResourceRead(const std::string& name, const RenderPass& renderPass)
{
    m_resourceReads[name].push_back(renderPass.GetId());
}

void RenderGraph::RegisterResourceWrite(const std::string& name, const RenderPass& renderPass)
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
    CheckPhysicalResources();

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
    /*
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
    }*/
}

void RenderGraph::CreatePhysicalResources()
{
    struct ImageInfo
    {
        int32_t width  = -1;
        int32_t height = -1;
        ImageCreateInfo createInfo;
        std::vector<uint32_t> virtualResourceIds;
    };
    std::vector<ImageInfo> imageInfos;
    struct BufferCreateInfo
    {
        uint64_t size;
        VkBufferUsageFlags usage;
        VkMemoryPropertyFlags memoryFlags;
        std::vector<uint32_t> virtualResourceIds;
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
                }
            default:
                LOG_ERROR("Unsupported size modifier");
            }

            info.virtualResourceIds.push_back(m_resourceIds[resource->GetName()]);
            imageInfos.push_back(info);
        }
        else
        {
            imageInfos[resource->GetPhysicalId()].createInfo.usage |= resource->GetTextureInfo().usageFlags;
            imageInfos[resource->GetPhysicalId()].virtualResourceIds.push_back(m_resourceIds[resource->GetName()]);
        }
    };
    auto AddOrUpdateBufferInfo = [&](RenderingBufferResource* resource)
    {
        if(resource->GetPhysicalId() == -1)
        {
            auto bufferInfo = resource->GetBufferInfo();
            uint32_t id     = m_transientBuffers.size();
            resource->SetPhysicalId(id);

            BufferCreateInfo info{};
            info.size        = bufferInfo.size;
            info.usage       = bufferInfo.usageFlags;
            info.memoryFlags = bufferInfo.memoryFlags;
            info.virtualResourceIds.push_back(m_resourceIds[resource->GetName()]);
            bufferInfos.push_back(info);
        }
        else
        {
            bufferInfos[resource->GetPhysicalId()].usage       |= resource->GetBufferInfo().usageFlags;
            bufferInfos[resource->GetPhysicalId()].memoryFlags |= resource->GetBufferInfo().memoryFlags;
            bufferInfos[resource->GetPhysicalId()].virtualResourceIds.push_back(m_resourceIds[resource->GetName()]);
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

        for(auto* input : pass->GetDrawCommandBuffers())
        {
            AddOrUpdateBufferInfo(input);
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

    m_transientImages.reserve(imageInfos.size());
    m_transientBuffers.reserve(bufferInfos.size());
    for(auto& info : imageInfos)
    {
        auto* ptr = &m_transientImages.emplace_back(info.width, info.height, info.createInfo);

        for(auto id : info.virtualResourceIds)
        {
            static_cast<RenderingTextureResource*>(m_resources[id].get())->SetImagePointer(ptr);
        }
    }
    for(auto& info : bufferInfos)
    {
        auto* ptr = &m_transientBuffers.emplace_back(info.size, info.usage, info.memoryFlags);

        for(auto id : info.virtualResourceIds)
        {
            static_cast<RenderingBufferResource*>(m_resources[id].get())->SetBufferPointer(ptr);
        }
    }

    // TODO merge the usages of the virtual resources that have been merged together into the same physical resource
    for(const auto& info : imageInfos)
    {
        std::unordered_map<uint32_t, std::pair<VkPipelineStageFlags2, VkAccessFlags2>> usages;
        for(auto id : info.virtualResourceIds)
        {
            for(auto [passId, usage] : m_resources[id]->GetUsages())
            {
                auto [stages, access]  = usage;
                usages[passId].first  |= stages;
                usages[passId].second |= access;
            }
        }
        for(auto id : info.virtualResourceIds)
        {
            m_resources[id]->m_uses = usages;
        }
    }

    for(const auto& info : bufferInfos)
    {
        std::unordered_map<uint32_t, std::pair<VkPipelineStageFlags2, VkAccessFlags2>> usages;
        for(auto id : info.virtualResourceIds)
        {
            for(auto [passId, usage] : m_resources[id]->GetUsages())
            {
                auto [stages, access]  = usage;
                usages[passId].first  |= stages;
                usages[passId].second |= access;
            }
        }
        for(auto id : info.virtualResourceIds)
        {
            m_resources[id]->m_uses = usages;
        }
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

void RenderGraph::CheckPhysicalResources()
{
    bool ok = true;
    for(auto& resource : m_resources)
    {
        switch(resource->GetType())
        {
        case RenderingResource::Type::Texture:
            {
                auto* texture = static_cast<RenderingTextureResource*>(resource.get());
                if(!texture->GetImagePointer())
                {
                    LOG_ERROR("Texture resource {0} has no image assigned", texture->GetName());
                    ok = false;
                }
            }
            break;
        case RenderingResource::Type::Buffer:
            {
                auto* buffer = static_cast<RenderingBufferResource*>(resource.get());
                if(!buffer->GetBufferPointer())
                {
                    LOG_ERROR("Buffer resource {0} has no buffer assigned", buffer->GetName());
                    ok = false;
                }
            }
            break;
        }
    }
    if(!ok)
        throw std::runtime_error("Physical resources are not valid");
}


inline VkImageLayout ConvertAccessToLayout(VkAccessFlags2 access)
{
    if(access & (VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT))
        return VK_IMAGE_LAYOUT_GENERAL;

    // attachment with write access
    if(access & (VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT))
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

inline bool IsDepthFormat(VkFormat format)
{
    return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}
inline bool HasStencil(VkFormat format)
{
    return format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

bool HasWriteAccess(VkAccessFlags2 access)
{
    return access & (VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT);
}

void RenderGraph::AddSynchronization()
{
    // just a helper function to avoid code duplication

    /*
    auto SetupImageSync = [&](uint32_t srcIndex, RenderingTextureResource* resource, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess)
    {
        // keep track of the stages we already synced this resource write to
        // in order to avoid redundant syncyng
        VkPipelineStageFlags2 alreadySyncedToStages = 0;

        uint32_t srcPassId      = m_orderedPasses[srcIndex]->GetId();
        bool srcReadOnly        = !HasWriteAccess(srcAccess);
        VkImageLayout srcLayout = ConvertAccessToLayout(srcAccess);


        for(uint32_t dstIndex = srcIndex + 1; dstIndex < m_orderedPasses.size(); ++dstIndex)
        {
            auto it = resource->GetUsages().find(m_orderedPasses[dstIndex]->GetId());
            if(it == resource->GetUsages().end())
                continue;

            auto [stages, usage]    = it->second;
            uint32_t dstPassId      = m_orderedPasses[dstIndex]->GetId();
            bool dstReadOnly        = !HasWriteAccess(usage);
            VkImageLayout dstLayout = ConvertAccessToLayout(usage);
            VkImage img             = resource->GetImagePointer()->GetImage();

            // if read to read and no layout transition then we don't need to sync
            if(srcReadOnly && dstReadOnly && srcLayout == dstLayout)
                continue;

            if(stages & alreadySyncedToStages)
                continue;

            // find and delete any events from passes that are earlier in the order and remove the barrier corresponging to this resource from them because we no longer need it
            // because current srcpass is using the same resource as that previous pass so the two must be synced with eachother, and this means that dstpass will still be synced with the previous pass by a synch chain through the current srcpass
            for(uint32_t k = 0; k < srcindex; ++k)
            {
                auto it = m_events.find({m_orderedpasses[k]->getid(), dstpassid});
                if(it != m_events.end())
                {
                    // r(prev)->r(src)->w(dst) and no layout transition between the rs then we remove r1->w (in case its a layout transition) but we need to add back the sync from r1->w because since r1 and r2 aren't synced there wouldn't be a sync chain syncing r1 with w so we add r1's stage and access flag to the r2->w sync
                    // in addition, by doing this we basically regroup the syncing of a chain of reads (without layout transitions) into a single sync at the end of the chain
                    auto prevusageit = resource->getusages().find(m_orderedpasses[k]->getid());
                    if(prevusageit == resource->getusages().end())
                        continue;

                    vkaccessflags2 prevaccess = prevusageit->second.second;
                    if(!haswriteaccess(prevaccess) && srcreadonly && !dstreadonly && (convertaccesstolayout(prevaccess) == srclayout))
                    {
                        vkimagememorybarrier2 barrier;
                        if(it->second.getbarrier(img, barrier) && !haswriteaccess(barrier.srcaccessmask))
                        {
                            stages |= barrier.srcstagemask;
                            usage |= barrier.srcaccessmask;
                        }
                    }
                    it->second.removebarrier(resource->getimagepointer()->getimage());
                }
            }


            VkImageMemoryBarrier2 barrier{};
            barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.srcStageMask        = srcStage;
            barrier.dstStageMask        = stages;
            barrier.srcAccessMask       = srcAccess;
            barrier.dstAccessMask       = usage;
            barrier.oldLayout           = srcLayout;
            barrier.newLayout           = dstLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image               = img;

            if(IsDepthFormat(resource->GetImagePointer()->GetFormat()))
            {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                if(HasStencil(resource->GetImagePointer()->GetFormat()))
                    barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            else
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;


            // if it's used in the next pass we should use a normal pipeline barrier because apparently using a VkEvent isn't as good for performance as a normal barrier if we set it then immediately wait for it
            if(dstIndex == srcIndex + 1)
            {
                m_imageBarriers[srcPassId].push_back(barrier);
            }
            else  // use an event so work between the two passes can be done without waiting for the synchronisation
            {
                // it is fine to do it like this instead of using find because if the event doesn't exist yet we would want to create it anyway, so the map inserting it when using the [] operator is fine because VulkanEvent's default constructor does the vkCreateEvent call
                auto& event = m_events[{srcPassId, dstPassId}];
                event.AddBarrier(barrier);
            }
            alreadySyncedToStages |= stages;
        }
    };
*/
    auto SetupImageSync = [&](uint32_t srcIndex, RenderingTextureResource* resource, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess)
    {
        // a resource can only be transitioned once after a pass, now matter how many times it is used in a different layout, so we transition it into the layout that is used first and we add the stage/access masks of further passes that use it in the same layout as what we transition it to up until it should be transitioned into a different layout, after that we don't sync
        VkImageMemoryBarrier2 transitionBarrier{};
        transitionBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

        uint32_t srcPassId            = m_orderedPasses[srcIndex]->GetId();
        VkImageLayout srcLayout       = ConvertAccessToLayout(srcAccess);
        bool srcReadOnly              = !HasWriteAccess(srcAccess);
        bool first                    = true;
        VkImageLayout nextLayout      = VK_IMAGE_LAYOUT_UNDEFINED;
        int32_t transitionIndex       = -1;
        VkImage img                   = resource->GetImagePointer()->GetImage();
        VkImageAspectFlags aspectMask = 0;

        if(IsDepthFormat(resource->GetImagePointer()->GetFormat()))
        {
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if(HasStencil(resource->GetImagePointer()->GetFormat()))
                aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        else
            aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;


        // find all read accesses that happened before srcpass because we need to add them to the transitionbarrier.srcStage/Accesses in case there is a transition right after srcpass
        // need to do this because read to read aren't synced so there wouldn't be a sync chain through srcPass
        if(srcReadOnly)
        {
            for(uint32_t k = 0; k < srcIndex; ++k)
            {
                auto it = resource->GetUsages().find(m_orderedPasses[k]->GetId());
                if(it == resource->GetUsages().end())
                    continue;

                auto [stages, usage] = it->second;
                if(!HasWriteAccess(usage) && srcReadOnly && (ConvertAccessToLayout(usage) == srcLayout))
                {
                    transitionBarrier.srcStageMask  |= stages;
                    transitionBarrier.srcAccessMask |= usage;
                }
            }
        }

        for(uint32_t dstIndex = srcIndex + 1; dstIndex < m_orderedPasses.size(); ++dstIndex)
        {
            uint32_t dstPassId = m_orderedPasses[dstIndex]->GetId();
            auto it            = resource->GetUsages().find(dstPassId);

            if(it == resource->GetUsages().end())
                continue;

            auto [stages, usage]    = it->second;
            VkImageLayout dstLayout = ConvertAccessToLayout(usage);
            bool dstReadOnly        = !HasWriteAccess(usage);

            VkPipelineStageFlags2 batchedSrcStage = srcStage;
            VkAccessFlags2 batchedSrcAccess       = srcAccess;

            for(uint32_t k = 0; k < srcIndex; ++k)
            {
                auto it = m_events.find({m_orderedPasses[k]->GetId(), dstPassId});
                if(it != m_events.end())
                {
                    // R(prev)->R(src)->X(dst) and no layout transition between the Rs then we remove R1->X but we need to add back the sync from R1->W because since R1 and R2 aren't synced there wouldn't be a sync chain syncing R1 with W so we add R1's stage and access flag to the R2->W sync
                    // in addition, by doing this we basically regroup the syncing of a chain of reads (without layout transitions) into a single sync at the end of the chain
                    auto prevUsageIt = resource->GetUsages().find(m_orderedPasses[k]->GetId());
                    if(prevUsageIt == resource->GetUsages().end())
                        continue;

                    VkAccessFlags2 prevAccess = prevUsageIt->second.second;
                    if(!HasWriteAccess(prevAccess) && srcReadOnly && (ConvertAccessToLayout(prevAccess) == srcLayout))
                    {
                        VkImageMemoryBarrier2 barrier;
                        if(it->second.GetBarrier(img, barrier) && !HasWriteAccess(barrier.srcAccessMask))
                        {
                            batchedSrcStage  |= barrier.srcStageMask;
                            batchedSrcAccess |= barrier.srcAccessMask;
                        }
                    }
                    it->second.RemoveBarrier(resource->GetImagePointer()->GetImage());
                }
            }

            if(first)
            {
                nextLayout = dstLayout;
                if(srcLayout != dstLayout)
                {
                    transitionBarrier.srcStageMask        |= batchedSrcStage;
                    transitionBarrier.srcAccessMask       |= batchedSrcAccess;
                    transitionBarrier.dstStageMask         = stages;
                    transitionBarrier.dstAccessMask        = usage;
                    transitionBarrier.oldLayout            = srcLayout;
                    transitionBarrier.newLayout            = dstLayout;
                    transitionBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
                    transitionBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
                    transitionBarrier.image                = img;

                    transitionBarrier.subresourceRange.aspectMask     = aspectMask;
                    transitionBarrier.subresourceRange.baseMipLevel   = 0;
                    transitionBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
                    transitionBarrier.subresourceRange.baseArrayLayer = 0;
                    transitionBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

                    transitionIndex = dstIndex;
                }
            }
            if(nextLayout != dstLayout)
                break;

            if(transitionIndex != -1)
            {
                transitionBarrier.dstStageMask  |= stages;
                transitionBarrier.dstAccessMask |= usage;
            }
            else
            {
                if(srcReadOnly && dstReadOnly)
                    continue;

                VkImageMemoryBarrier2 barrier{};
                barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                barrier.srcStageMask        = batchedSrcStage;
                barrier.dstStageMask        = stages;
                barrier.srcAccessMask       = batchedSrcAccess;
                barrier.dstAccessMask       = usage;
                barrier.oldLayout           = srcLayout;  // srcLayout should be equal to dstLayout
                barrier.newLayout           = dstLayout;  // so no transition
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image               = img;

                barrier.subresourceRange.aspectMask     = aspectMask;
                barrier.subresourceRange.baseMipLevel   = 0;
                barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

                if(dstIndex == srcIndex + 1)
                {
                    m_imageBarriers[srcPassId].push_back(barrier);
                }
                else
                {
                    auto& event = m_events[{srcPassId, dstPassId}];
                    event.AddBarrier(barrier);
                }
            }

            first = false;
            // we can stop syncing to passes after the first write, because later passes will be synced to the pass that writes anyway so we'd just be insterting redundant sync
            if(!dstReadOnly)
                break;
        }


        if(transitionIndex != -1)
        {
            if(srcIndex + 1 == transitionIndex)
            {
                m_imageBarriers[srcPassId].push_back(transitionBarrier);
            }
            else
            {
                auto& event = m_events[{srcPassId, m_orderedPasses[transitionIndex]->GetId()}];
                event.AddBarrier(transitionBarrier);
            }
        }
    };
    auto SetupBufferSync
        = [&](uint32_t i, RenderingBufferResource* resource, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess)
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

            uint32_t dstPassId = m_orderedPasses[j]->GetId();

            // see comment in SetupImageSync for why we do this
            for(uint32_t k = 0; k < j; ++k)
            {
                auto it = m_events.find({m_orderedPasses[k]->GetId(), dstPassId});
                if(it != m_events.end())
                    it->second.RemoveBarrier(resource->GetBufferPointer()->GetVkBuffer());
            }

            auto [stages, usage] = it->second;
            if(stages & alreadySyncedStages)
                continue;


            VkBufferMemoryBarrier2 barrier{};
            barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barrier.srcStageMask        = srcStage;
            barrier.dstStageMask        = stages;
            barrier.srcAccessMask       = srcAccess;
            barrier.dstAccessMask       = usage;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer              = resource->GetBufferPointer()->GetVkBuffer();
            barrier.offset              = 0;
            barrier.size                = VK_WHOLE_SIZE;


            // if it's used in the next pass we should use a normal pipeline barrier because apparently using a VkEvent isn't as good for performance as a normal barrier if we set it then immediately wait for it
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

    m_imageBarriers.resize(m_renderPasses.size());
    m_bufferBarriers.resize(m_renderPasses.size());

    for(uint32_t i = 0; i < m_orderedPasses.size(); ++i)
    {
        for(auto* resource : m_orderedPasses[i]->GetColorOutputs())
        {
            SetupImageSync(i, resource, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        }
        for(auto* resource : m_orderedPasses[i]->GetColorInputs())
        {
            if(!resource)
                continue;
            SetupImageSync(i, resource, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);
        }

        auto* depthOutput = m_orderedPasses[i]->GetDepthOutput();
        if(depthOutput)
        {
            SetupImageSync(i, depthOutput, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        }
        auto* depthInput = m_orderedPasses[i]->GetDepthInput();
        if(depthInput)
        {
            SetupImageSync(i, depthInput, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
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
        for(auto* resource : m_orderedPasses[i]->GetStorageBufferInputs())
        {
            if(!resource)
                continue;

            auto [srcStage, _] = resource->GetUsages()[m_orderedPasses[i]->GetId()];

            SetupBufferSync(i, resource, srcStage, VK_ACCESS_2_SHADER_READ_BIT);
        }
        for(auto* resource : m_orderedPasses[i]->GetUniformBufferInputs())
        {
            if(!resource)
                continue;

            auto [srcStage, _] = resource->GetUsages()[m_orderedPasses[i]->GetId()];

            SetupBufferSync(i, resource, srcStage, VK_ACCESS_2_UNIFORM_READ_BIT);
        }
        for(auto* resource : m_orderedPasses[i]->GetTextureInputs())
        {
            auto [srcStage, _] = resource->GetUsages()[m_orderedPasses[i]->GetId()];

            SetupImageSync(i, resource, srcStage, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        }
    }


    // put the pointers in the correct places to facilitate finding the events to signal/wait on during the execute loop
    m_eventSignals.resize(m_orderedPasses.size());
    m_eventWaits.resize(m_orderedPasses.size());
    for(auto& [passes, event] : m_events)
    {
        const auto& [srcPassId, dstPassId] = passes;
        // remove the event if it doesn't have any barriers (this can be the case because we potentially remove barriers one by one in the SetupXSync functions)
        if(event.Empty())
        {
            m_events.erase(passes);
            continue;
        }
        event.Allocate();  // actually create the event now that we know that it will be used
        m_eventSignals[srcPassId].push_back(&event);
        m_eventWaits[dstPassId].push_back(&event);
    }

    auto* renderTargetResource = m_resources[m_resourceIds[SWAPCHAIN_RESOURCE_NAME]].get();
    for(uint32_t i = m_orderedPasses.size() - 1; i >= 0; --i)
    {
        auto it = renderTargetResource->GetUsages().find(m_orderedPasses[i]->GetId());
        if(it != renderTargetResource->GetUsages().end())
        {
            auto [stages, usage]      = it->second;
            m_finalRenderTargetLayout = ConvertAccessToLayout(usage);
            break;
        }
    }
}

void RenderGraph::InitialisePasses()
{
    for(auto& pass : m_orderedPasses)
    {
        pass->Initialise();
    }
}

void RenderGraph::Execute(CommandBuffer& cb, const uint32_t frameIndex)
{
    assert(m_isBuilt);
    // we don't allow reading from the swapchain image, I don't think it makes sense
    /*
    for(uint32_t passId : m_resourceWrites[SWAPCHAIN_RESOURCE_NAME])
    {
        m_renderPasses[passId]->SetSwapchainImageView(m_swapchainImages[frameIndex].GetImageView());
    }
    */

    auto& renderTarget = m_transientImages[m_resources[m_resourceIds[SWAPCHAIN_RESOURCE_NAME]]->GetPhysicalId()];


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
        renderTargetBarrier.newLayout                       = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
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
        
        // TODO remove
        VkImageMemoryBarrier tmpDepthBarrier{};
        tmpDepthBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        tmpDepthBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        tmpDepthBarrier.newLayout                       = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        tmpDepthBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        tmpDepthBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        tmpDepthBarrier.image                           = m_transientImages[m_resources[m_resourceIds["depthImage"]]->GetPhysicalId()].GetImage();
        tmpDepthBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
        tmpDepthBarrier.subresourceRange.baseMipLevel   = 0;
        tmpDepthBarrier.subresourceRange.baseArrayLayer = 0;
        tmpDepthBarrier.subresourceRange.layerCount     = 1;
        tmpDepthBarrier.subresourceRange.levelCount     = 1;
        tmpDepthBarrier.srcAccessMask                   = 0;
        tmpDepthBarrier.dstAccessMask                   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(cb.GetCommandBuffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &renderTargetBarrier);
        vkCmdPipelineBarrier(cb.GetCommandBuffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, nullptr, 0, nullptr, 1, &tmpDepthBarrier);
    }

    for(auto& pass : m_orderedPasses)
    {
        std::vector<VkEvent> events;
        std::vector<VkDependencyInfo> dependencies;
        for(auto* event : m_eventWaits[pass->GetId()])
        {
            events.push_back(event->GetEvent());
            dependencies.push_back(event->GetDependencyInfo());
        }
        if(!events.empty())
            vkCmdWaitEvents2(cb.GetCommandBuffer(), events.size(), events.data(), dependencies.data());

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

        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.dependencyFlags          = 0;
        dependencyInfo.bufferMemoryBarrierCount = m_bufferBarriers[pass->GetId()].size();
        dependencyInfo.pBufferMemoryBarriers    = m_bufferBarriers[pass->GetId()].data();
        dependencyInfo.imageMemoryBarrierCount  = m_imageBarriers[pass->GetId()].size();
        dependencyInfo.pImageMemoryBarriers     = m_imageBarriers[pass->GetId()].data();

        vkCmdPipelineBarrier2(cb.GetCommandBuffer(), &dependencyInfo);

        for(auto* event : m_eventSignals[pass->GetId()])
        {
            event->Set(cb);
        }
    }

    for(auto& passEvents : m_eventSignals)
    {
        for(auto* event : passEvents)
        {
            event->Reset(cb);
        }
    }
    for(auto& passEvents : m_eventWaits)
    {
        for(auto* event : passEvents)
        {
            event->Reset(cb);
        }
    }


    // Transfer the render target into transfer src optimal layout
    {
        VkImageMemoryBarrier renderTargetBarrier{};
        renderTargetBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        renderTargetBarrier.oldLayout                       = m_finalRenderTargetLayout;
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
    auto AddImageBarrierToTooltip = [&](std::string& tooltip, VkImageMemoryBarrier2 imgBarrier)
    {
        tooltip += "Image barrier:\\n";
        tooltip += "   srcStage: ";
        tooltip += string_VkPipelineStageFlags2(imgBarrier.srcStageMask);
        tooltip += "\\n";
        tooltip += "   srcAccess: ";
        tooltip += string_VkAccessFlags2(imgBarrier.srcAccessMask);
        tooltip += "\\n";
        tooltip += "   dstStage: ";
        tooltip += string_VkPipelineStageFlags2(imgBarrier.dstStageMask);
        tooltip += "\\n";
        tooltip += "   dstAccess: ";
        tooltip += string_VkAccessFlags2(imgBarrier.dstAccessMask);
        tooltip += "\\n";
        tooltip += "   oldLayout: ";
        tooltip += string_VkImageLayout(imgBarrier.oldLayout);
        tooltip += "\\n";
        tooltip += "   newLayout: ";
        tooltip += string_VkImageLayout(imgBarrier.newLayout);
        tooltip += "\\n";
        tooltip += "   srcQueueFamily: ";
        tooltip += std::to_string(imgBarrier.srcQueueFamilyIndex);
        tooltip += "\\n";
        tooltip += "   dstQueueFamily: ";
        tooltip += std::to_string(imgBarrier.dstQueueFamilyIndex);
        tooltip += "\\n";
        tooltip += "   aspect: ";
        tooltip += string_VkImageAspectFlags(imgBarrier.subresourceRange.aspectMask);
        tooltip += "\\n";
    };

    auto AddBufferBarrierToTooltip
        = [&](std::string& tooltip, VkBufferMemoryBarrier2 bufferBarrier)
    {
        tooltip += "Buffer barrier:\\n";
        tooltip += "   srcStage: ";
        tooltip += string_VkPipelineStageFlags2(bufferBarrier.srcStageMask);
        tooltip += "\\n";
        tooltip += "   srcAccess: ";
        tooltip += string_VkAccessFlags2(bufferBarrier.srcAccessMask);
        tooltip += "\\n";
        tooltip += "   dstStage: ";
        tooltip += string_VkPipelineStageFlags2(bufferBarrier.dstStageMask);
        tooltip += "\\n";
        tooltip += "   dstAccess: ";
        tooltip += string_VkAccessFlags2(bufferBarrier.dstAccessMask);
        tooltip += "\\n";
        tooltip += "   srcQueueFamily: ";
        tooltip += std::to_string(bufferBarrier.srcQueueFamilyIndex);
        tooltip += "\\n";
        tooltip += "   dstQueueFamily: ";
        tooltip += std::to_string(bufferBarrier.dstQueueFamilyIndex);
        tooltip += "\\n";
    };


    std::ofstream file(filename);
    file << "digraph G {\n";
    for(uint32_t node = 0; node < m_graph.size(); ++node)
    {
        for(const auto& edge : m_graph[node])
        {
            file << m_renderPasses[node]->GetName() << " -> " << m_renderPasses[edge]->GetName();
            std::string syncMethod;
            std::string tooltip;
            if(m_events.find({node, edge}) != m_events.end())
            {
                syncMethod += "Event, ";
                for(auto imgBarrier : m_events[{node, edge}].GetImageBarriers())
                {
                    AddImageBarrierToTooltip(tooltip, imgBarrier);
                }
                for(auto bufferBarrier : m_events[{node, edge}].GetBufferBarriers())
                {
                    AddBufferBarrierToTooltip(tooltip, bufferBarrier);
                }
            }
            else
            {
                if(!m_imageBarriers[node].empty())
                {
                    syncMethod += "ImageBarrier, ";
                    for(auto imgBarrier : m_imageBarriers[node])
                    {
                        AddImageBarrierToTooltip(tooltip, imgBarrier);
                    }
                }
                if(!m_bufferBarriers[node].empty())
                {
                    syncMethod += "BufferBarrier, ";
                    for(auto bufferBarrier : m_bufferBarriers[node])
                    {
                        AddBufferBarrierToTooltip(tooltip, bufferBarrier);
                    }
                }
            }
            if(!syncMethod.empty())
            {
                file << "[label=\"" << syncMethod << "\"][labeltooltip=\"" << tooltip << "\"]";
            }
            file << "\n";
        }
    }
    for(uint32_t i = 0; i < m_orderedPasses.size(); ++i)
    {
        auto* pass = m_orderedPasses[i];
        file << pass->GetName() << "[label=\"" << i + 1 << ": " << pass->GetName() << "\"]\n";
    }
    file << "}";
    file.close();
}
