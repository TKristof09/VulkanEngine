#pragma once
#include <vulkan/vulkan.h>
#include <string.h>
#include <vector>

class Shader
{
public:
    Shader(const std::string& filename);
    VkShaderModule GetShaderModule(VkDevice device);

private:
    std::vector<char> m_data;

};
