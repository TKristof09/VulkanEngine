#pragma once
#include <vulkan/vulkan.h>

class Shader
{
public:
    Shader(const std::string& filename);
    VkShaderModule GetShaderModule(VkDevice device);

private:
    std::vector<char> m_data;

};
