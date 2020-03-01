#pragma once
#include <vulkan/vulkan.hpp>

class Shader
{
public:
    Shader(const std::string& filename);
    vk::ShaderModule GetShaderModule(vk::Device device);

private:
    std::vector<char> m_data;

};