#pragma once
#include <vulkan/vulkan.h>
#include "RenderPass.hpp"
#include "Rendering/Image.hpp"

class Image;
class Buffer;

class RenderingResource
{
public:
    enum class Type
    {
        Texture,
        Buffer,
        TextureArray  // runtime texture array - size isnt known at rendergraph compile time (e.g. shadowmap array) - this means that this has to be external lifetime
    };
    enum class Lifetime
    {
        Transient,
        External
    };

    RenderingResource(Type type, const std::string& name)
        : m_type(type), m_name(name) {}

    [[nodiscard]] Type GetType() const { return m_type; }
    [[nodiscard]] Lifetime GetLifetime() const { return m_lifetime; }
    [[nodiscard]] std::string GetName() const { return m_name; }

    [[nodiscard]] uint32_t GetPhysicalId() const { return m_physicalId; }

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

    void AddUse(uint32_t passId, VkPipelineStageFlags2 stages, VkAccessFlags2 usage)
    {
        m_uses[passId] = {stages, usage};
    }

    [[nodiscard]] const std::unordered_map<uint32_t, std::pair<VkPipelineStageFlags2, VkAccessFlags2>>& GetUsages() const { return m_uses; }

private:
    friend class RenderGraph;

    void SetPhysicalId(uint32_t id) { m_physicalId = id; }

    Type m_type;
    Lifetime m_lifetime = Lifetime::Transient;
    std::string m_name;
    QueueTypeFlags m_queueTypes = 0;
    uint32_t m_lastAcess        = 0;
    uint32_t m_firstAccess      = UINT32_MAX;
    int32_t m_physicalId        = -1;

    std::unordered_map<uint32_t, std::pair<VkPipelineStageFlags2, VkAccessFlags2>> m_uses;
};


struct TextureInfo
{
    SizeModifier sizeModifier = SizeModifier::SwapchainRelative;
    float width               = 1.0f;
    float height              = 1.0f;

    VkFormat format;
    VkImageUsageFlags usageFlags;
    VkImageLayout layout;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkClearValue clearValue;
    bool clear = false;

    void SetClearValue(VkClearValue color)
    {
        clearValue = color;
        clear      = true;
    }
};
class RenderingTextureResource : public RenderingResource
{
public:
    RenderingTextureResource(const std::string& name)
        : RenderingResource(Type::Texture, name) {}

    // TODO: This should OR the flags probably
    void SetTextureInfo(const TextureInfo& textureInfo) { m_textureInfo = textureInfo; }
    void SetImagePointer(Image* image) { m_image = image; }

    [[nodiscard]] TextureInfo GetTextureInfo() const { return m_textureInfo; }
    [[nodiscard]] Image* GetImagePointer() const { return m_image; }

private:
    TextureInfo m_textureInfo{};
    Image* m_image = nullptr;  // owned either by the rendergraph (for transient resources) or by the user (for external resources)
};

struct BufferInfo
{
    uint64_t size;
    VkPipelineStageFlags2 stageFlags;
    VkBufferUsageFlags usageFlags;
    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    void operator|=(const BufferInfo& other)
    {
        size         = std::max(size, other.size);
        stageFlags  |= other.stageFlags;
        usageFlags  |= other.usageFlags;
        memoryFlags |= other.memoryFlags;
    }
};
class RenderingBufferResource : public RenderingResource
{
public:
    RenderingBufferResource(const std::string& name)
        : RenderingResource(Type::Buffer, name) {}

    void SetBufferInfo(const BufferInfo& bufferInfo) { m_bufferInfo |= bufferInfo; }
    void SetBufferPointer(Buffer* buffer) { m_buffer = buffer; }

    [[nodiscard]] BufferInfo GetBufferInfo() const { return m_bufferInfo; }
    [[nodiscard]] Buffer* GetBufferPointer() const { return m_buffer; }

private:
    BufferInfo m_bufferInfo{};
    Buffer* m_buffer = nullptr;  // owned either by the rendergraph (for transient resources) or by the user (for external resources)
};

// All the images have to have the same format, layout
class RenderingTextureArrayResource : public RenderingResource
{
public:
    RenderingTextureArrayResource(const std::string& name)
        : RenderingResource(Type::TextureArray, name)

    {
        SetLifetime(Lifetime::External);
    }

    void SetFormat(VkFormat format) { m_format = format; }
    [[nodiscard]] VkFormat GetFormat() const { return m_format; }
    void AddImagePointer(Image* image)
    {
        if(m_format != VK_FORMAT_UNDEFINED)
            assert(m_format == image->GetFormat());
        else
            m_format = image->GetFormat();

        m_images.push_back(image);
    }

    [[nodiscard]] const std::vector<Image*>& GetImagePointers() const { return m_images; }

private:
    VkFormat m_format{};
    std::vector<Image*> m_images;  // owned either by the rendergraph (for transient resources) or by the user (for external resources)
};
