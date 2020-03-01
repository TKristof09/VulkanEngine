#include "Shader.hpp"
#include <fstream>


Shader::Shader(const std::string& filename)
{
    std::ifstream file("../../../" + filename, std::ios::ate | std::ios::binary);
    if(!file.is_open())
        throw std::runtime_error("Failed to open file " + filename);

    size_t filesize = (size_t)file.tellg();
    m_data.resize(filesize);
    file.seekg(0);
    file.read(m_data.data(), filesize);
    file.close();
}

vk::ShaderModule Shader::GetShaderModule(vk::Device device)
{
    vk::ShaderModuleCreateInfo createInfo;
    createInfo.codeSize                 = m_data.size();
    createInfo.pCode                    = reinterpret_cast<const uint32_t*>(m_data.data());

    return device.createShaderModule(createInfo);

    //could deallocate the data vector here
    //std::vector<char> tempVector;
    //m_data.swap(tempVector);

}