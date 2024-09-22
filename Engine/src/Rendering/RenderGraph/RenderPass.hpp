#pragma once

#include "Rendering/CommandBuffer.hpp"
#include <vulkan/vulkan.h>

#include <utility>

class RenderGraph;
class RenderingTextureResource;
class RenderingBufferResource;
class RenderingTextureArrayResource;

enum QueueTypeFlagBits
{
    Graphics = 1 << 0,
    Compute  = 1 << 1
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
    float width               = 1.0f;
    float height              = 1.0f;
    bool clear                = false;
    VkClearValue clearValue   = {};
};

constexpr uint32_t MAX_DRAW_COMMANDS = 10000;
struct DrawCommand
{
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t objectID;
};
class RenderPass
{
public:
    RenderPass(RenderGraph& graph, uint32_t id, QueueTypeFlagBits type)
        : m_graph(graph), m_id(id), m_type(type), m_renderingInfo({}) {}


    RenderingTextureResource& AddColorOutput(const std::string& name, AttachmentInfo attachmentInfo, const std::string& input = "");
    RenderingTextureResource& AddDepthInput(const std::string& name);
    RenderingTextureResource& AddDepthOutput(const std::string& name, AttachmentInfo attachmentInfo);

    RenderingBufferResource& AddDrawCommandBuffer(const std::string& name);

    RenderingBufferResource& AddVertexBufferInput(const std::string& name);
    RenderingBufferResource& AddIndexBufferInput(const std::string& name);

    RenderingBufferResource& AddUniformBufferInput(const std::string& name, VkPipelineStageFlags2 stages = 0, bool external = false);
    RenderingTextureResource& AddTextureInput(const std::string& name, bool external = false);

    RenderingBufferResource& AddStorageBufferReadOnly(const std::string& name, VkPipelineStageFlags2 stages = 0, bool external = false);
    RenderingBufferResource& AddStorageBufferOutput(const std::string& name, const std::string& input = "", VkPipelineStageFlags2 stages = 0, bool external = false);

    RenderingTextureResource& AddStorageImageReadOnly(const std::string& name, VkPipelineStageFlags2 stages = 0, bool external = false);
    RenderingTextureResource& AddStorageImageOutput(const std::string& name, const std::string& input = "", VkPipelineStageFlags2 stages = 0, VkFormat format = VK_FORMAT_UNDEFINED, bool external = false);
    RenderingTextureResource& AddStorageImageOutput(const std::string& name, VkFormat format);

    RenderingTextureArrayResource& AddTextureArrayInput(const std::string& name, VkPipelineStageFlags2 stages, VkAccessFlags2 access);
    RenderingTextureArrayResource& AddTextureArrayOutput(const std::string& name, VkPipelineStageFlags2 stages, VkAccessFlags2 access);


    void SetInitialiseCallback(std::function<void(RenderGraph&)> callback) { m_initialiseCallback = std::move(callback); }
    void SetExecutionCallback(std::function<void(CommandBuffer&, uint32_t)> callback) { m_executionCallback = std::move(callback); }

    // This happens AFTER the physical resources have been created by the rendergraph
    // This is where you should create descriptor sets for example
    // This is optional
    void Initialise()
    {
        if(m_initialiseCallback)
            m_initialiseCallback(m_graph);
    }
    // This gets called every frame, this is the "draw" function
    // This is required
    void Execute(CommandBuffer& commandBuffer, uint32_t imageIndex) { m_executionCallback(commandBuffer, imageIndex); }

    // Only one attachment is allowed to be for the swapchain image
    void AddAttachmentInfo(VkRenderingAttachmentInfo info, bool isSwapchain = false)
    {
        if(isSwapchain)
            m_swapchainAttachmentIndex = static_cast<int32_t>(m_attachmentInfos.size());
        m_attachmentInfos.push_back(info);
    }
    void SetRenderingInfo(VkRenderingInfo info) { m_renderingInfo = info; }
    void SetName(const std::string& name) { m_name = name; }


    // Getter for inputs and outputs
    [[nodiscard]] const std::vector<RenderingTextureResource*>& GetColorInputs() const { return m_colorInputs; }
    [[nodiscard]] const std::vector<RenderingTextureResource*>& GetColorOutputs() const { return m_colorOutputs; }
    [[nodiscard]] RenderingTextureResource* GetDepthInput() const { return m_depthInput; }
    [[nodiscard]] RenderingTextureResource* GetDepthOutput() const { return m_depthOutput; }

    [[nodiscard]] const std::vector<RenderingBufferResource*>& GetDrawCommandBuffers() const { return m_drawCommandBuffers; }

    [[nodiscard]] const std::vector<RenderingBufferResource*>& GetVertexBufferInputs() const { return m_vertexBufferInputs; }
    [[nodiscard]] const std::vector<RenderingBufferResource*>& GetIndexBufferInputs() const { return m_indexBufferInputs; }

    [[nodiscard]] const std::vector<RenderingBufferResource*>& GetUniformBufferInputs() const { return m_uniformBufferInputs; }
    [[nodiscard]] const std::vector<RenderingTextureResource*>& GetTextureInputs() const { return m_textureInputs; }

    [[nodiscard]] const std::vector<RenderingBufferResource*>& GetStorageBufferInputs() const { return m_storageBufferInputs; }
    [[nodiscard]] const std::vector<RenderingBufferResource*>& GetStorageBufferOutputs() const { return m_storageBufferOutputs; }

    [[nodiscard]] const std::vector<RenderingTextureResource*>& GetStorageImageInputs() const { return m_storageImageInputs; }
    [[nodiscard]] const std::vector<RenderingTextureResource*>& GetStorageImageOutputs() const { return m_storageImageOutputs; }

    [[nodiscard]] const std::vector<RenderingTextureArrayResource*>& GetTextureArrayInputs() const { return m_textureArrayInputs; }
    [[nodiscard]] const std::vector<RenderingTextureArrayResource*>& GetTextureArrayOutputs() const { return m_textureArrayOutputs; }

    [[nodiscard]] uint32_t GetId() const { return m_id; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }

    [[nodiscard]] const std::vector<VkRenderingAttachmentInfo>& GetAttachmentInfos() const { return m_attachmentInfos; }

    void SetSwapchainImageView(VkImageView imageView)
    {
        m_attachmentInfos[m_swapchainAttachmentIndex].imageView = imageView;
    }
    [[nodiscard]] const VkRenderingInfo* GetRenderingInfo() const { return &m_renderingInfo; }
    [[nodiscard]] QueueTypeFlagBits GetType() const { return m_type; }


private:
    RenderingBufferResource& AddBufferInput(const std::string& name, VkDeviceSize size, VkPipelineStageFlags2 stageFlags, VkBufferUsageFlags usageFlags);

    RenderGraph& m_graph;
    uint32_t m_id;
    QueueTypeFlagBits m_type;
    std::string m_name;

    std::function<void(RenderGraph&)> m_initialiseCallback;
    std::function<void(CommandBuffer&, uint32_t)> m_executionCallback;

    // resources are owned by the rendergraph
    std::vector<RenderingTextureResource*> m_colorInputs;
    std::vector<RenderingTextureResource*> m_colorOutputs;
    RenderingTextureResource* m_depthInput  = nullptr;
    RenderingTextureResource* m_depthOutput = nullptr;

    // use vector because a pass can use multiple graphics pipelines (aka shaders) so each can have their own draw commands
    std::vector<RenderingBufferResource*> m_drawCommandBuffers;

    std::vector<RenderingBufferResource*> m_vertexBufferInputs;
    std::vector<RenderingBufferResource*> m_indexBufferInputs;

    std::vector<RenderingBufferResource*> m_uniformBufferInputs;
    std::vector<RenderingTextureResource*> m_textureInputs;

    std::vector<RenderingBufferResource*> m_storageBufferInputs;
    std::vector<RenderingBufferResource*> m_storageBufferOutputs;

    std::vector<RenderingTextureResource*> m_storageImageInputs;
    std::vector<RenderingTextureResource*> m_storageImageOutputs;

    std::vector<RenderingTextureArrayResource*> m_textureArrayInputs;
    std::vector<RenderingTextureArrayResource*> m_textureArrayOutputs;

    std::vector<VkRenderingAttachmentInfo> m_attachmentInfos;
    int32_t m_swapchainAttachmentIndex = -1;
    VkRenderingInfo m_renderingInfo;
};
