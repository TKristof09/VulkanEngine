#pragma once

#include "RenderPass.hpp"
#include "Rendering/Image.hpp"
#include "Rendering/Buffer.hpp"
#include "RenderingResource.hpp"
#include "vulkan/vulkan_core.h"
#include <stack>
#include <unordered_set>
#include <vulkan/vulkan.h>

/*
typedef std::string ResourceHandle;
enum AccessType
{
    Read,
    Write,
    ReadWrite
};
struct Node
{
    std::string name;
    std::unordered_map<ResourceHandle, AccessType> buffers;
    std::unordered_map<ResourceHandle, AccessType> images;
    void (*Execute)(std::unordered_map<ResourceHandle, AccessType>&, std::unordered_map<ResourceHandle, AccessType>&);
};*/

#define SWAPCHAIN_RESOURCE_NAME "__swapchain"
#define NUM_FRAMES_IN_FLIGHT 2

class RenderPass2;
class RenderGraph
{
public:

    RenderGraph():
        m_swapchainResource(RenderingTextureResource(SWAPCHAIN_RESOURCE_NAME))
    {
        TextureInfo info = {};
        VkClearValue clearValue = {};
        clearValue.color = {0.0f, 0.0f, 0.0f, 1.0f};
        info.SetClearValue(clearValue);
        m_swapchainResource.SetTextureInfo(info);
    }

    void SetupSwapchainImages(const std::vector<VkImage>& swapchainImages);
    RenderPass2& AddRenderPass(const std::string& name, QueueTypeFlagBits type);

    RenderingTextureResource& GetTextureResource(const std::string& name);
    RenderingBufferResource& GetBufferResource(const std::string& name);

    void RegisterResourceRead(const std::string& name, const RenderPass2& renderPass);
    void RegisterResourceWrite(const std::string& name, const RenderPass2& renderPass);

    void Build();

    Image& GetPhysicalImage(const RenderingResource& resource)
    {
        assert(resource.GetLifetime() == RenderingResource::Lifetime::Transient);
        return m_transientImages[resource.GetPhysicalId()];
    }
    Buffer& GetPhysicalBuffer(const RenderingResource& resource)
    {
        assert(resource.GetLifetime() == RenderingResource::Lifetime::Transient);
        return m_transientBuffers[resource.GetPhysicalId()];

    }

    void Execute(CommandBuffer& cb, uint32_t frameIndex);
private:
    void ToDOT(const std::string& filename);

    void CreateEdges();
    void OrderPasses();
    void FindResourceLifetimes();
    void CreatePhysicalResources();
    void CreatePhysicalPasses();
    void AddSynchronization();
    void InitialisePasses();




    void TopologicalSortUtil(uint32_t currentNode, std::stack<uint32_t>& stack, std::unordered_set<uint32_t>& visited);

    std::vector<std::unordered_set<int32_t>> m_graph;

    std::vector<std::unique_ptr<RenderingResource>> m_ressources;
    std::unordered_map<std::string, uint32_t> m_resourceIds;
    std::unordered_map<std::string, std::vector<uint32_t>> m_resourceReads;
    std::unordered_map<std::string, std::vector<uint32_t>> m_resourceWrites;

    // TODO RenderPasses are not owned by the graph?
    std::vector<RenderPass2*> m_renderPasses;
    // same as m_renderPasses but ordered based on the dependencies between passes, to be used for execution
    std::vector<RenderPass2*> m_orderedPasses;
    std::unordered_map<std::string, uint32_t> m_renderPassIds;



    std::vector<Image> m_transientImages;
    std::vector<Buffer> m_transientBuffers;


    std::vector<Image> m_swapchainImages; // TODO: maybe keep the Renderer as the owner of these?
    RenderingTextureResource m_swapchainResource;


    bool m_isBuilt = false;

};
