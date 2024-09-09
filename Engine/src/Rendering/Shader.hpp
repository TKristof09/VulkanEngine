#pragma once
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <string>
#include <vector>
#include "Texture.hpp"

class Pipeline;

class Shader
{
public:
    Shader(const std::string& filename, VkShaderStageFlagBits stage, Pipeline* pipeline);
    ~Shader()
    {
        DestroyShaderModule();
    }

    Shader(const Shader& other) = delete;

    Shader(Shader&& other) noexcept
        : m_shaderModule(other.m_shaderModule),
          m_stage(other.m_stage)
    {
        other.m_shaderModule = VK_NULL_HANDLE;
    }

    Shader& operator=(const Shader& other) = delete;

    Shader& operator=(Shader&& other) noexcept
    {
        if(this == &other)
            return *this;
        m_shaderModule       = other.m_shaderModule;
        other.m_shaderModule = VK_NULL_HANDLE;
        m_stage              = other.m_stage;
        return *this;
    }

private:
    friend class Renderer;
    friend class Pipeline;

    std::vector<uint32_t> Compile(const std::filesystem::path& path);

    void Reflect(const std::string& filename, std::vector<uint32_t>* data, Pipeline* pipeline);

    VkShaderModule GetShaderModule() const { return m_shaderModule; };

    void DestroyShaderModule()
    {
        if(m_shaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(VulkanContext::GetDevice(), m_shaderModule, nullptr);
            m_shaderModule = VK_NULL_HANDLE;
        }
    }
    VkShaderModule m_shaderModule;
    VkShaderStageFlagBits m_stage;
};
