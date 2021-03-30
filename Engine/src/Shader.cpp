#include "Shader.hpp"
#include <fstream>
#include <spirv_cross.hpp>
#include "VulkanContext.hpp"
#include <filesystem>
#include <vulkan/vulkan_core.h>
#include <Pipeline.hpp>


Shader::Shader(const std::string& filename, VkShaderStageFlagBits stage)
{
	std::vector<uint32_t> data;
	{

		std::filesystem::path path;
		if(stage & VK_SHADER_STAGE_VERTEX_BIT)
		{
			path = "./shaders/" + filename + ".vert.spv";
			m_stage = VK_SHADER_STAGE_VERTEX_BIT;
		}
		else if(stage & VK_SHADER_STAGE_FRAGMENT_BIT)
		{
			path = "./shaders/" + (filename + ".frag.spv");
			m_stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		LOG_TRACE("");
		LOG_TRACE("Loading {0}", path);

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
	LOG_TRACE("-----PUSH CONSTANTS-----");

	for(auto& resource : resources.push_constant_buffers)
	{
		spirv_cross::SPIRType type = comp.get_type(resource.type_id);
		uint32_t size = comp.get_declared_struct_size(type);
		uint32_t offset = comp.get_decoration(resource.id, spv::DecorationOffset);

		LOG_TRACE(resource.name);
		LOG_TRACE("   Size: {0}", size);
		LOG_TRACE("   Offset: {0}", offset);

		m_pushConstants.used = true;
		m_pushConstants.size = size;
		m_pushConstants.offset = offset;

	}

	LOG_TRACE("-----UNIFORM BUFFERS-----");
	for(auto& resource : resources.uniform_buffers)
	{
		spirv_cross::SPIRType type = comp.get_type(resource.type_id);
		uint32_t set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
	    uint32_t binding = comp.get_decoration(resource.id, spv::DecorationBinding);
		uint32_t location = comp.get_decoration(resource.id, spv::DecorationLocation);
		uint32_t size = comp.get_declared_struct_size(type);
		uint32_t arraySize = type.array[0];

		if(type.array.size() > 1)
			LOG_WARN("{0} is an array of {1} dimensions in shader: {2}", resource.name, type.array.size(), filename);

		if(type.array.size() == 0)
		{
			LOG_WARN("{0} is an array of {1} dimensions in shader: {2}, setting count to 1", resource.name, type.array.size(), filename);
			arraySize = 1;
		}

		LOG_TRACE(resource.name);
		LOG_TRACE("   Size: {0}", size);
		LOG_TRACE("   Location: {0}", location);
		LOG_TRACE("   Set: {0}", set);
		LOG_TRACE("   Binding: {0}", binding);
		LOG_TRACE("   Count: {0}", arraySize);

		m_uniformBuffers[resource.name] = UniformBufferInfo();

		m_uniformBuffers[resource.name].stage = stage;
		m_uniformBuffers[resource.name].size = size;
		m_uniformBuffers[resource.name].location = location;
		m_uniformBuffers[resource.name].binding = binding;
		m_uniformBuffers[resource.name].set = set;
		m_uniformBuffers[resource.name].count = arraySize;

	}

	uint32_t maxBinding = 0;
	std::string maxName = "";

	LOG_TRACE("-----IMAGE SAMPLERS-----");
	for(auto& resource : resources.sampled_images)
	{
		spirv_cross::SPIRType type = comp.get_type(resource.type_id);
	    uint32_t binding = comp.get_decoration(resource.id, spv::DecorationBinding);
		uint32_t set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
		uint32_t arraySize = type.array[0];

		if(type.array.size() > 1)
			LOG_WARN("{0} is an array of {1} dimensions in shader: {2}", resource.name, type.array.size(), filename);

		if(type.array.size() == 0)
		{
			LOG_WARN("{0} is an array of {1} dimensions in shader: {2}, setting count to 1", resource.name, type.array.size(), filename);
			arraySize = 1;
		}

		if(arraySize == 0)
		{
			LOG_WARN("{0} has a count of 0 setting it to 1, this is most likely because the array uses variable indexing", resource.name);
			arraySize = 1;
		}

		if(arraySize != OBJECTS_PER_DESCRIPTOR_CHUNK)
			LOG_WARN("{0}'s count is different than OBJECTS_PER_DESCRIPTOR_CHUNK({1})", resource.name, OBJECTS_PER_DESCRIPTOR_CHUNK);

		LOG_TRACE(resource.name);
		LOG_TRACE("   Set: {0}", set);
		LOG_TRACE("   Binding: {0}", binding);
		LOG_TRACE("   Count: {0}", arraySize);
		m_textures[resource.name] = TextureInfo();

		m_textures[resource.name].stage = stage;
		m_textures[resource.name].binding = binding;
		m_textures[resource.name].set = set;
		m_textures[resource.name].count = arraySize;


		if(binding > maxBinding)
		{
			maxBinding = binding;
			maxName = resource.name;
		}

	}

	if(maxName != "")
		m_textures[maxName].isLast = true;



	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType					= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize                 = data.size() * sizeof(uint32_t);
    createInfo.pCode                    = data.data();

    VK_CHECK(vkCreateShaderModule(VulkanContext::GetDevice(), &createInfo, nullptr, &m_shaderModule), "Failed to create shader module");
}

