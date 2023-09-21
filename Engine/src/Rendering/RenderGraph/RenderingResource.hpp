#pragma once
#include <stdint.h>
#include <vulkan/vulkan.h>
#include "RenderPass.hpp"
#include "vulkan/vulkan_core.h"

class RenderingResource
{
public:
    enum class Type
    {
        Texture,
        Buffer
    };
    enum class Lifetime
    {
        Transient,
        External
    };
    RenderingResource(Type type, const std::string& name)
        : m_type(type), m_lifetime(Lifetime::Transient) , m_name(name){ }

    Type GetType() const { return m_type; }
    Lifetime GetLifetime() const { return m_lifetime; }
    std::string GetName() const { return m_name; }

    uint32_t GetPhysicalId() const { return m_physicalId; }

    void SetLifetime(Lifetime lifetime) { m_lifetime = lifetime; }

    void AddQueueUse(QueueTypeFlags queueType)
    {
        m_queueTypes |= queueType;
    }

    // Not Renderpass::id, but the index in the rendergraph's ordered list of renderpasses
    void AddAccess(uint32_t idx)
    {
        if(idx > m_lastAcess)
            m_lastAcess = idx;
        if(idx < m_firstAccess)
            m_firstAccess = idx;
    }

private:
    friend class RenderGraph;

    void SetPhysicalId(uint32_t id) { m_physicalId = id; }

    Type m_type;
    Lifetime m_lifetime;
    std::string m_name;
    QueueTypeFlags m_queueTypes;
    uint32_t m_lastAcess = 0;
    uint32_t m_firstAccess = UINT32_MAX;
    int32_t m_physicalId = -1;
};


struct TextureInfo
{
    SizeModifier sizeModifier = SizeModifier::SwapchainRelative;
    float width = 1.0f;
    float height = 1.0f;

    VkFormat format;
    VkImageUsageFlags usageFlags;
    VkImageAspectFlags aspectFlags;
    VkImageLayout layout;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkClearValue clearValue;
    bool clear = false;

    void SetClearValue(VkClearValue color)
    {
        clearValue = color;
        clear = true;
    }
};
class RenderingTextureResource : public RenderingResource
{
public:
    RenderingTextureResource(const std::string& name)
        : RenderingResource(Type::Texture, name) { }

    void SetTextureInfo(const TextureInfo& textureInfo) { m_textureInfo = textureInfo; }
    TextureInfo GetTextureInfo() const { return m_textureInfo; }

private:
    TextureInfo m_textureInfo;
};

struct BufferInfo
{
    uint64_t size;
    VkPipelineStageFlags stageFlags;
    VkAccessFlags accessFlags;
    VkBufferUsageFlags usageFlags;
    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
};
class RenderingBufferResource : public RenderingResource
{
public:
    RenderingBufferResource(const std::string& name)
        : RenderingResource(Type::Buffer, name) { }

    void SetBufferInfo(const BufferInfo& bufferInfo) { m_bufferInfo = bufferInfo; }

    BufferInfo GetBufferInfo() const { return m_bufferInfo; }
private:
    BufferInfo m_bufferInfo;
};
