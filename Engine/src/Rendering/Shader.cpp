#include "Shader.hpp"
#include <fstream>
#include <spirv_cross.hpp>
#include "VulkanContext.hpp"
#include <filesystem>
#include <vulkan/vulkan_core.h>
#include "Rendering/Pipeline.hpp"


Shader::Shader(const std::string& filename, VkShaderStageFlagBits stage, Pipeline* pipeline)
{
    std::vector<uint32_t> data;
    {
        std::filesystem::path path;

        switch(stage)
        {
        case VK_SHADER_STAGE_VERTEX_BIT:
            path = "./shaders/" + filename + ".vert.spv";
            break;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            path = "./shaders/" + (filename + ".frag.spv");
            break;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            path = "./shaders/" + (filename + ".comp.spv");
            break;
        default:
            LOG_ERROR("Shader stage not supported for shader: {0}", filename);
            break;
        }
        m_stage = stage;

        LOG_TRACE("");
        LOG_TRACE("Loading {0}", path);

        // TODO make this work better(not hardcodign file path)
        std::ifstream file(path.c_str(), std::ios::ate | std::ios::binary);
        if(!file.is_open())
            throw std::runtime_error("Failed to open file " + filename);

        std::vector<char> temp;
        size_t filesize = (size_t)file.tellg();
        temp.resize(filesize);
        file.seekg(0, std::ios::beg);
        file.read(temp.data(), filesize);
        file.close();

        // TODO figure out how to do this better
        data = std::vector<uint32_t>(reinterpret_cast<uint32_t*>(temp.data()), reinterpret_cast<uint32_t*>(temp.data()) + filesize / sizeof(uint32_t));
    }

    Reflect(filename, &data, pipeline);

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize                 = data.size() * sizeof(uint32_t);
    createInfo.pCode                    = data.data();

    VK_CHECK(vkCreateShaderModule(VulkanContext::GetDevice(), &createInfo, nullptr, &m_shaderModule), "Failed to create shader module");
}

void Shader::Reflect(const std::string& filename, std::vector<uint32_t>* data, Pipeline* pipeline)
{
    spirv_cross::Compiler comp(*data);

    spirv_cross::ShaderResources resources = comp.get_shader_resources();


    if(m_stage == VK_SHADER_STAGE_VERTEX_BIT)
    {
        uint32_t bindingSize = 0;
        LOG_TRACE("-----VERTEX ATTRIBUTES-----");
        std::vector<std::pair<VkVertexInputAttributeDescription, uint32_t>> attributeDescriptions;
        attributeDescriptions.resize(resources.stage_inputs.size());
        for(auto& resource : resources.stage_inputs)
        {
            spirv_cross::SPIRType type = comp.get_type(resource.type_id);
            uint32_t location          = comp.get_decoration(resource.id, spv::DecorationLocation);
            uint32_t size              = type.vecsize * type.width / 8;  // size in bytes
            uint32_t offset            = comp.get_decoration(resource.id, spv::DecorationOffset);

            LOG_TRACE(resource.name);
            LOG_TRACE("   Location: {0}", location);
            LOG_TRACE("   Size: {0}", size);
            LOG_TRACE("   Offset: {0}", offset);

            VkVertexInputAttributeDescription attribDescription = {};
            attribDescription.binding                           = 0;  // only support one binding for now
            attribDescription.location                          = location;
            switch(size)
            {
            case 4:
                attribDescription.format = VK_FORMAT_R32_SFLOAT;
                break;
            case 8:
                attribDescription.format = VK_FORMAT_R32G32_SFLOAT;
                break;
            case 12:
                attribDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
                break;
            case 16:
                attribDescription.format = VK_FORMAT_R32G32B32A32_SFLOAT;
                break;
            }
            // attribDescription.offset = offset; offset is always zero for some reason
            // so we store them in a vector and do another pass to calculate the offsets
            attributeDescriptions[location] = {attribDescription, size};

            bindingSize += size;
        }
        uint32_t currentOffset = 0;
        for(auto& [attribDescription, size] : attributeDescriptions)
        {
            attribDescription.offset  = currentOffset;
            currentOffset            += size;
            pipeline->m_vertexInputAttributes.push_back(attribDescription);
        }


        if(bindingSize > 0)
        {
            // only support one binding for now
            VkVertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding                         = 0;
            bindingDescription.stride                          = bindingSize;
            bindingDescription.inputRate                       = VK_VERTEX_INPUT_RATE_VERTEX;

            pipeline->m_vertexInputBinding = bindingDescription;
        }
    }

    LOG_TRACE("-----PUSH CONSTANTS-----");

    for(auto& resource : resources.push_constant_buffers)
    {
        spirv_cross::SPIRType type = comp.get_type(resource.type_id);
        uint32_t size              = comp.get_declared_struct_size(type);
        uint32_t offset            = comp.get_decoration(resource.id, spv::DecorationOffset);

        LOG_TRACE(resource.name);
        LOG_TRACE("   Size: {0}", size);
        LOG_TRACE("   Offset: {0}", offset);
    }

    if(!resources.sampled_images.empty())
    {
        pipeline->m_usesDescriptorSet = true;
    }

    if(!resources.storage_images.empty())
    {
        pipeline->m_usesDescriptorSet = true;
    }
}
