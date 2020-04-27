#pragma once
#include <vulkan/vulkan.h>

class Shader
{
public:
    Shader(const eastl::string& filename);
    VkShaderModule GetShaderModule(VkDevice device);

private:
    eastl::vector<char> m_data;

};
