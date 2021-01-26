#include "Shader.hpp"
#include <fstream>
#include <spirv_cross.hpp>
#include "VulkanContext.hpp"
#include <filesystem>


Shader::Shader(const std::string& filename, VkPipelineStageFlags stage)
{
	std::vector<uint32_t> data;
	{

		std::filesystem::path path;
		if(stage & VK_SHADER_STAGE_VERTEX_BIT)
			path = "./shaders/" + filename + ".vert.spv";
		else if(stage & VK_SHADER_STAGE_FRAGMENT_BIT)
			path = "./shaders/" + (filename + ".frag.spv");

		// TODO make this work better(not hardcodign file path)
		std::ifstream file(path.c_str(), std::ios::ate | std::ios::binary);
		if(!file.is_open())
			throw std::runtime_error(("Failed to open file " + filename).c_str());

		std::vector<char> temp;
		size_t filesize = (size_t)file.tellg();
		temp.resize(filesize);
		file.seekg(0, std::ios::beg);
		file.read(temp.data(), filesize);
		file.close();

		// TODO figure out how to do this better
		data = std::vector<uint32_t> (reinterpret_cast<uint32_t*>(temp.data()), reinterpret_cast<uint32_t*>(temp.data())+filesize/sizeof(uint32_t));
	}

	spirv_cross::Compiler comp(data);

	spirv_cross::ShaderResources resources = comp.get_shader_resources();
	LOG_TRACE("");
	LOG_TRACE("Loading {0}", filename);
	for(auto& resource : resources.uniform_buffers)
	{
		spirv_cross::SPIRType type = comp.get_type(resource.type_id);
		uint32_t set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
	    uint32_t binding = comp.get_decoration(resource.id, spv::DecorationBinding);
		uint32_t location = comp.get_decoration(resource.id, spv::DecorationLocation);
		uint32_t size = comp.get_declared_struct_size(type);
		uint32_t arraySize = type.array[0];

		if(type.array.size() != 1)
			LOG_WARN("{0} is an array of {1} dimensions in {2}", resource.name, type.array.size(), filename);


		LOG_TRACE("-----UNIFORM BUFFERS-----");
		LOG_TRACE(resource.name);
		LOG_TRACE("   Size: {0}", size);
		LOG_TRACE("   Location: {0}", location);
		LOG_TRACE("   Binding: {0}", binding);
		LOG_TRACE("   Set: {0}", set);

		m_uniformBuffers[resource.name] = UniformBufferInfo();

		m_uniformBuffers[resource.name].stage = stage;
		m_uniformBuffers[resource.name].size = size;
		m_uniformBuffers[resource.name].location = location;
		m_uniformBuffers[resource.name].binding = binding;
		m_uniformBuffers[resource.name].set = set;
		m_uniformBuffers[resource.name].count = arraySize;

	}
	for(auto& resource : resources.sampled_images)
	{
		spirv_cross::SPIRType type = comp.get_type(resource.type_id);
	    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
		uint32_t arraySize = type.array[0];

		if(type.array.size() != 1)
			LOG_WARN("{0} is an array of {1} dimensions in {2}", resource.name, type.array.size(), filename);

		LOG_TRACE("-----IMAGE SAMPLERS-----");
		LOG_TRACE(resource.name);
		LOG_TRACE("   Binding: {0}", binding);
		m_Textures[resource.name] = TextureInfo();

		m_Textures[resource.name].stage = stage;
		m_Textures[resource.name].binding = binding;
		m_Textures[resource.name].count = arraySize;
	}



	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType					= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize                 = data.size() * sizeof(uint32_t);
    createInfo.pCode                    = data.data();

    VK_CHECK(vkCreateShaderModule(VulkanContext::GetDevice(), &createInfo, nullptr, &m_shaderModule), "Failed to create shader module");
}

