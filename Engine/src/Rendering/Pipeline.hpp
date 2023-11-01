#pragma once

#include "VulkanContext.hpp"
#include "Shader.hpp"
#include "vulkan/vulkan.h"


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
    bool useDynamicState  = false;  // not supported yet


    std::vector<VkFormat> colorFormats;
    VkFormat depthFormat   = VK_FORMAT_D32_SFLOAT;
    VkFormat stencilFormat = VK_FORMAT_S8_UINT;  // TODO look into stencil stuff

    VkShaderStageFlags stages = 0;
    VkExtent2D viewportExtent = {};

    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    bool depthWriteEnable      = false;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;

    bool depthClampEnable = false;

    bool isGlobal = false;
};

class Pipeline
{
public:
    Pipeline(const std::string& shaderName, PipelineCreateInfo createInfo, uint16_t priority = std::numeric_limits<uint16_t>::max());
    ~Pipeline();

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
          m_isGlobal(other.m_isGlobal),
          m_name(std::move(other.m_name)),
          m_shaders(std::move(other.m_shaders)),
          m_pipeline(other.m_pipeline),
          m_layout(other.m_layout),
          m_materialBufferPtr(other.m_materialBufferPtr),
          m_uniformBuffers(std::move(other.m_uniformBuffers)),
          m_textures(std::move(other.m_textures)),
          m_storageImages(std::move(other.m_storageImages)),
          m_storageBuffers(std::move(other.m_storageBuffers))
    {
        other.m_pipeline = VK_NULL_HANDLE;
    }

    Pipeline& operator=(const Pipeline& other) = delete;

    Pipeline& operator=(Pipeline&& other) noexcept
    {
        if(this == &other)
            return *this;
        m_priority          = other.m_priority;
        m_isGlobal          = other.m_isGlobal;
        m_name              = std::move(other.m_name);
        m_shaders           = std::move(other.m_shaders);
        m_pipeline          = other.m_pipeline;
        m_layout            = other.m_layout;
        m_materialBufferPtr = other.m_materialBufferPtr;
        m_uniformBuffers    = std::move(other.m_uniformBuffers);
        m_textures          = std::move(other.m_textures);
        m_storageImages     = std::move(other.m_storageImages);
        m_storageBuffers    = std::move(other.m_storageBuffers);

        other.m_pipeline = VK_NULL_HANDLE;
        return *this;
    }

    [[nodiscard]] uint64_t GetMaterialBufferPtr() const { return m_materialBufferPtr; }

private:
    friend class Renderer;
    friend class MaterialSystem;
    friend class DescriptorSetAllocator;
    friend class Shader;

    void CreateGraphicsPipeline(const PipelineCreateInfo& createInfo);
    void CreateComputePipeline(const PipelineCreateInfo& createInfo);


    uint16_t m_priority;
    bool m_isGlobal;
    std::string m_name;
    std::vector<Shader> m_shaders;
    VkPipeline m_pipeline;
    VkPipelineLayout m_layout;

    uint64_t m_materialBufferPtr = 0;


    struct BufferInfo
    {
        VkPipelineStageFlags stages;

        size_t size;

        uint32_t binding;
        uint32_t set;
        uint32_t count;
    };
    struct TextureInfo
    {
        VkPipelineStageFlags stages;

        uint32_t binding;
        uint32_t set;
        uint32_t count;
    };
    std::unordered_map<std::string, BufferInfo> m_uniformBuffers;
    std::unordered_map<std::string, TextureInfo> m_textures;
    std::unordered_map<std::string, TextureInfo> m_storageImages;
    std::unordered_map<std::string, BufferInfo> m_storageBuffers;
};
