#include "Shader.hpp"
#include <fstream>


Shader::Shader(const std::string& filename)
{
	// TODO make this work better(not hardcodign file path)
    std::ifstream file(("./" + filename).c_str(), std::ios::ate | std::ios::binary);
    if(!file.is_open())
        throw std::runtime_error(("Failed to open file " + filename).c_str());

    size_t filesize = (size_t)file.tellg();
    m_data.resize(filesize);
    file.seekg(0);
    file.read(m_data.data(), filesize);
    file.close();
}

VkShaderModule Shader::GetShaderModule(VkDevice device)
{
    VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType					= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize                 = m_data.size();
    createInfo.pCode                    = reinterpret_cast<const uint32_t*>(m_data.data());

	VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule), "Failed to create shader module");
	return shaderModule;
    //could deallocate the data vector here
    //std::vector<char> tempVector;
    //m_data.swap(tempVector);

}
