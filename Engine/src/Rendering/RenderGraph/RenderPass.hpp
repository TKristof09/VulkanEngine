#pragma once

#include "Rendering/CommandBuffer.hpp"
#include <vulkan/vulkan.h>

class RenderGraph;
class RenderingTextureResource;
class RenderingBufferResource;

enum QueueTypeFlagBits
{
    Graphics = 1 << 0,
    Compute = 1 << 1
};
using QueueTypeFlags = uint32_t;

enum SizeModifier
{
    Absolute,
    SwapchainRelative
};
struct AttachmentInfo
{
    SizeModifier sizeModifier = SizeModifier::SwapchainRelative;
    float width = 1.0f;
    float height = 1.0f;
    bool clear = false;
    VkClearValue clearValue = {};
};
class RenderPass2
{
public:
    RenderPass2(RenderGraph& graph, uint32_t id, QueueTypeFlagBits type)
        : m_graph(graph), m_id(id), m_type(type) { }


    RenderingTextureResource& AddColorOutput(const std::string& name, AttachmentInfo attachmentInfo,const std::string& input = "");
    RenderingTextureResource& AddDepthInput(const std::string& name);
    RenderingTextureResource& AddDepthOutput(const std::string& name, AttachmentInfo attachmentInfo);
    RenderingTextureResource& AddResolveOutput(const std::string& name);
    RenderingTextureResource& AddDepthResolveOutput(const std::string& name);

    RenderingBufferResource& AddVertexBufferInput(const std::string& name);
    RenderingBufferResource& AddIndexBufferInput(const std::string& name);

    RenderingBufferResource& AddUniformBufferInput(const std::string& name, VkPipelineStageFlags stages = 0, bool external = false);
    RenderingTextureResource& AddTextureInput(const std::string& name, bool external = false);

    RenderingBufferResource& AddStorageBufferReadOnly(const std::string& name, bool external = false);
    RenderingBufferResource& AddStorageBufferOutput(const std::string& name, const std::string& input = "", bool external = false);


    void SetInitialiseCallback(std::function<void(RenderGraph&)> callback) { m_initialiseCallback = callback; }
    void SetExecutionCallback(std::function<void(CommandBuffer&, uint32_t)> callback) { m_executionCallback = callback; }

    // This happens AFTER the physical resources have been created by the rendergraph
    // This is where you should create descriptor sets for example
    // This is optional
    void Initialise() { if(m_initialiseCallback) m_initialiseCallback(m_graph); }
    // This gets called every frame, this is the "draw" function
    // This is required
    void Execute(CommandBuffer& commandBuffer, uint32_t imageIndex) { m_executionCallback(commandBuffer, imageIndex); }

    // Only one attachment is allowed to be for the swapchain image
    void AddAttachmentInfo(VkRenderingAttachmentInfo info, bool isSwapchain=false)
    {
        if(isSwapchain)
            m_swapchainAttachmentIndex = m_attachmentInfos.size();
        m_attachmentInfos.push_back(info);
    }
    void SetRenderingInfo(VkRenderingInfo info) { m_renderingInfo = info; }
    void SetName(const std::string& name) { m_name = name; }



    // Getter for inputs and outputs
    const std::vector<RenderingTextureResource*>& GetColorInputs() const { return m_colorInputs; }
    const std::vector<RenderingTextureResource*>& GetColorOutputs() const { return m_colorOutputs; }
    RenderingTextureResource* GetDepthInput() const { return m_depthInput; }
    RenderingTextureResource* GetDepthOutput() const { return m_depthOutput; }
    const std::vector<RenderingTextureResource*>& GetResolveOutputs() const { return m_resolveOutputs; }
    RenderingTextureResource* GetDepthResolveOutput() const { return m_depthResolveOutput; }

    const std::vector<RenderingBufferResource*>& GetVertexBufferInputs() const { return m_vertexBufferInputs; }
    const std::vector<RenderingBufferResource*>& GetIndexBufferInputs() const { return m_indexBufferInputs; }

    const std::vector<RenderingBufferResource*>& GetUniformBufferInputs() const { return m_uniformBufferInputs; }
    const std::vector<RenderingTextureResource*>& GetTextureInputs() const { return m_textureInputs; }

    const std::vector<RenderingBufferResource*>& GetStorageBufferInputs() const { return m_storageBufferInputs; }
    const std::vector<RenderingBufferResource*>& GetStorageBufferOutputs() const { return m_storageBufferOutputs; }

    const uint32_t GetId() const { return m_id; }
    const std::string& GetName() const { return m_name; }

    const std::vector<VkRenderingAttachmentInfo>& GetAttachmentInfos() const { return m_attachmentInfos; }

    void SetSwapchainImageView(VkImageView imageView)
    {
        m_attachmentInfos[m_swapchainAttachmentIndex].imageView = imageView;
    }
    const VkRenderingInfo* GetRenderingInfo() const { return &m_renderingInfo; }
    const QueueTypeFlagBits GetType() const { return m_type; }

private:
    RenderingBufferResource& AddBufferInput(const std::string& name, VkPipelineStageFlags stageFlags, VkAccessFlags accessFlags, VkBufferUsageFlags usageFlags);

    RenderGraph& m_graph;
    uint32_t m_id;
    QueueTypeFlagBits m_type;
    std::string m_name;

    std::function<void(RenderGraph&)> m_initialiseCallback;
    std::function<void(CommandBuffer&, uint32_t)> m_executionCallback;

    std::vector<RenderingTextureResource*> m_colorInputs;
    std::vector<RenderingTextureResource*> m_colorOutputs;
    RenderingTextureResource* m_depthInput = nullptr;
    RenderingTextureResource* m_depthOutput = nullptr;
    std::vector<RenderingTextureResource*> m_resolveOutputs;
    RenderingTextureResource* m_depthResolveOutput = nullptr;

    std::vector<RenderingBufferResource*> m_vertexBufferInputs;
    std::vector<RenderingBufferResource*> m_indexBufferInputs;

    std::vector<RenderingBufferResource*> m_uniformBufferInputs;
    std::vector<RenderingTextureResource*> m_textureInputs;

    std::vector<RenderingBufferResource*> m_storageBufferInputs;
    std::vector<RenderingBufferResource*> m_storageBufferOutputs;

    std::vector<VkRenderingAttachmentInfo> m_attachmentInfos;
    int32_t m_swapchainAttachmentIndex = -1;
    VkRenderingInfo m_renderingInfo;
};
