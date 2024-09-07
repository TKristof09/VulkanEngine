#pragma once

#include "RenderPass.hpp"
#include "Rendering/Image.hpp"
#include "Rendering/Buffer.hpp"
#include "Rendering/Synchronization.hpp"
#include "RenderingResource.hpp"
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

class RenderPass;
class Renderer;
class RenderGraph
{
public:
    RenderGraph(Renderer* renderer)
        : m_swapchainResource(RenderingTextureResource(SWAPCHAIN_RESOURCE_NAME)),
          m_renderer(renderer)
    {
        TextureInfo info        = {};
        VkClearValue clearValue = {};
        clearValue.color        = {0.0f, 0.0f, 0.0f, 1.0f};
        info.SetClearValue(clearValue);
        m_swapchainResource.SetTextureInfo(info);
    }

    ~RenderGraph() = default;

    RenderGraph(const RenderGraph&)                = delete;
    RenderGraph& operator=(const RenderGraph&)     = delete;
    RenderGraph(RenderGraph&&) noexcept            = default;
    RenderGraph& operator=(RenderGraph&&) noexcept = default;

    void SetupSwapchainImages(const std::vector<VkImage>& swapchainImages);
    RenderPass& AddRenderPass(const std::string& name, QueueTypeFlagBits type);

    RenderingTextureResource& GetTextureResource(const std::string& name);
    RenderingBufferResource& GetBufferResource(const std::string& name);
    RenderingTextureArrayResource& GetTextureArrayResource(const std::string& name);

    void RegisterResourceRead(const std::string& name, const RenderPass& renderPass);
    void RegisterResourceWrite(const std::string& name, const RenderPass& renderPass);

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

    void Execute(CommandBuffer& cb, const uint32_t frameIndex);

private:
    void ToDOT(const std::string& filename);

    void CreateEdges();
    void OrderPasses();
    void FindResourceLifetimes();
    void CreatePhysicalResources();
    void CreatePhysicalPasses();
    void AddSynchronization();
    void CheckPhysicalResources();
    void InitialisePasses();


    void TopologicalSortUtil(uint32_t currentNode, std::stack<uint32_t>& stack, std::unordered_set<uint32_t>& visited);

    Renderer* m_renderer;

    std::vector<std::unordered_set<int32_t>> m_graph;
    bool m_isBuilt = false;

    std::vector<std::unique_ptr<RenderingResource>> m_resources;
    std::unordered_map<std::string, uint32_t> m_resourceIds;
    std::unordered_map<std::string, std::vector<uint32_t>> m_resourceReads;
    std::unordered_map<std::string, std::vector<uint32_t>> m_resourceWrites;

    // TODO RenderPasses are not owned by the graph?
    std::vector<std::unique_ptr<RenderPass>> m_renderPasses;
    // same as m_renderPasses but ordered based on the dependencies between passes, to be used for execution
    std::vector<RenderPass*> m_orderedPasses;
    std::unordered_map<std::string, uint32_t> m_renderPassIds;


    std::vector<Image> m_transientImages;
    std::vector<Buffer> m_transientBuffers;


    std::vector<Image> m_swapchainImages;  // TODO: maybe keep the Renderer as the owner of these?
    RenderingTextureResource m_swapchainResource;

    VkImageLayout m_finalRenderTargetLayout{VK_IMAGE_LAYOUT_UNDEFINED};


    // list of all barriers that need to be executed after each pass, indexed by passId
    std::vector<std::vector<VkBufferMemoryBarrier2>> m_bufferBarriers;
    std::vector<std::vector<VkImageMemoryBarrier2>> m_imageBarriers;
    std::vector<std::vector<std::pair<VkImageMemoryBarrier2, RenderingTextureArrayResource*>>> m_imageArrayBarriers;

    struct PairHash
    {
        template<class T1, class T2>
        std::size_t operator()(const std::pair<T1, T2>& v) const
        {
            return std::hash<T1>()(v.first) ^ std::hash<T2>()(v.second) << 1;
        }
    };
    std::unordered_map<std::pair<uint32_t, uint32_t>, VulkanEvent, PairHash> m_events;  // srcPassId, dstPassId -> event
    // these are used for easier access to the events for signaling/waiting during the execute loop
    std::vector<std::vector<VulkanEvent*>> m_eventSignals;
    std::vector<std::vector<VulkanEvent*>> m_eventWaits;
};
