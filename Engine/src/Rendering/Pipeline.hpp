#pragma once

#include "Rendering/CommandBuffer.hpp"
#include "Rendering/Renderer.hpp"
#include "VulkanContext.hpp"
#include "Shader.hpp"
#include <vulkan/vulkan.h>
#include <optional>


enum class PipelineType
{
    GRAPHICS,
    COMPUTE
};

class Pipeline;
struct PipelineCreateInfo
{
    PipelineType type;

    bool allowDerivatives = false;
    Pipeline* parent      = nullptr;


    // for GRAPHICS
    bool useColor         = true;
    bool useDepth         = false;
    bool useStencil       = false;
    bool useColorBlend    = false;
    bool useMultiSampling = false;
    bool useTesselation   = false;  // not supported yet

    bool useDynamicViewport = false;


    std::vector<VkFormat> colorFormats;
    VkFormat depthFormat   = VK_FORMAT_D32_SFLOAT;
    VkFormat stencilFormat = VK_FORMAT_S8_UINT;  // TODO look into stencil stuff

    VkShaderStageFlags stages = 0;
    VkExtent2D viewportExtent = {};

    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    bool depthWriteEnable      = false;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;

    bool depthClampEnable = false;

    uint32_t viewMask = 0;

    bool isGlobal = false;
};


struct PipelineReloadEvent : public Event
{
    std::string name;
};

class Pipeline
{
public:
    Pipeline(const std::string& shaderName, PipelineCreateInfo createInfo, uint16_t priority = std::numeric_limits<uint16_t>::max());
    ~Pipeline();

    void OnPipelineReload(PipelineReloadEvent e);

    friend bool operator<(const Pipeline& lhs, const Pipeline& rhs)
    {
        if(lhs.m_priority == rhs.m_priority)
            return lhs.m_name < rhs.m_name;
        else
            return lhs.m_priority < rhs.m_priority;
    }

    Pipeline(const Pipeline& other) = delete;

    Pipeline(Pipeline&& other) noexcept
        : m_priority(other.m_priority),
          m_name(std::move(other.m_name)),
          m_shaders(std::move(other.m_shaders)),
          m_pipeline(other.m_pipeline),
          m_layout(other.m_layout),
          m_materialBufferPtr(other.m_materialBufferPtr),
          m_usesDescriptorSet(other.m_usesDescriptorSet),
          m_vertexInputAttributes(std::move(other.m_vertexInputAttributes)),
          m_vertexInputBinding(other.m_vertexInputBinding)
    {
        other.m_pipeline = VK_NULL_HANDLE;
    }

    Pipeline& operator=(const Pipeline& other) = delete;

    Pipeline& operator=(Pipeline&& other) noexcept
    {
        if(this == &other)
            return *this;
        m_priority              = other.m_priority;
        m_name                  = std::move(other.m_name);
        m_shaders               = std::move(other.m_shaders);
        m_pipeline              = other.m_pipeline;
        m_layout                = other.m_layout;
        m_materialBufferPtr     = other.m_materialBufferPtr;
        m_usesDescriptorSet     = other.m_usesDescriptorSet;
        m_vertexInputAttributes = std::move(other.m_vertexInputAttributes);
        m_vertexInputBinding    = other.m_vertexInputBinding;

        other.m_pipeline = VK_NULL_HANDLE;
        return *this;
    }

    void Bind(CommandBuffer& cb) const;
    void SetPushConstants(CommandBuffer& cb, void* data, uint32_t size, uint32_t offset = 0);
    template<typename T>
    void UploadShaderData(T* data, uint32_t imageIndex, uint32_t offset = 0, uint32_t size = sizeof(T));
    [[nodiscard]] uint64_t GetShaderDataBufferPtr(uint32_t imageIndex) const
    {
        return m_renderer->GetShaderDataBuffer().GetDeviceAddress(m_shaderDataSlots[imageIndex]);
    }
    [[nodiscard]] uint64_t GetMaterialBufferPtr() const;
    [[nodiscard]] uint32_t GetViewMask() const { return m_createInfo.viewMask; }

private:
    friend class Renderer;
    friend class MaterialSystem;
    friend class DescriptorSetAllocator;
    friend class Shader;

    void Setup();

    void CreateGraphicsPipeline();
    void CreateComputePipeline();

    [[nodiscard]] inline VkPipelineBindPoint GetBindPoint() const
    {
        switch(m_createInfo.type)
        {
        case PipelineType::GRAPHICS:
            return VK_PIPELINE_BIND_POINT_GRAPHICS;
        case PipelineType::COMPUTE:
            return VK_PIPELINE_BIND_POINT_COMPUTE;
        default:
            LOG_ERROR("Invalid pipeline type");
            return VK_PIPELINE_BIND_POINT_MAX_ENUM;
        }
    }

    Renderer* m_renderer;

    std::string m_name;
    PipelineCreateInfo m_createInfo;
    uint16_t m_priority;
    std::vector<Shader> m_shaders;


    VkPipeline m_pipeline;
    VkPipelineLayout m_layout;

    uint64_t m_materialBufferPtr = 0;


    bool m_usesDescriptorSet = false;

    std::vector<VkVertexInputAttributeDescription> m_vertexInputAttributes;
    std::optional<VkVertexInputBindingDescription> m_vertexInputBinding;  // only support one for now

    std::vector<uint64_t> m_shaderDataSlots;
};


template<typename T>
void Pipeline::UploadShaderData(T* data, uint32_t imageIndex, uint32_t offset, uint32_t size)
{
    if(offset + size > sizeof(T))
    {
        LOG_ERROR("Shader data upload out of bounds");
        return;
    }

    if(m_shaderDataSlots.empty())
    {
        for(uint32_t i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
        {
            uint64_t slot = m_renderer->GetShaderDataBuffer().Allocate(sizeof(T));  // for the allocation we use the size of the full struct
            m_shaderDataSlots.push_back(slot);
        }
    }
    assert(m_shaderDataSlots.size() == NUM_FRAMES_IN_FLIGHT && "Shader should only have one shaderdata per frame in flight");

    m_renderer->GetShaderDataBuffer().UploadData(m_shaderDataSlots[imageIndex], data, offset, size);
}
