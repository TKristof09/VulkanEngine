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
		data = std::vector<uint32_t> (reinterpret_cast<uint32_t*>(temp.data()), reinterpret_cast<uint32_t*>(temp.data())+filesize/sizeof(uint32_t));
	}

	Reflect(filename, &data, pipeline);

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType					= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize                 = data.size() * sizeof(uint32_t);
    createInfo.pCode                    = data.data();

    VK_CHECK(vkCreateShaderModule(VulkanContext::GetDevice(), &createInfo, nullptr, &m_shaderModule), "Failed to create shader module");
}

void Shader::Reflect(const std::string& filename, std::vector<uint32_t>* data, Pipeline* pipeline)
{
	spirv_cross::Compiler comp(*data);

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

		auto it = pipeline->m_pushConstants.find(resource.name);
		if(it != pipeline->m_pushConstants.end())
		{
			it->second.stages |= m_stage;
		}
		else
		{
			pipeline->m_pushConstants[resource.name].stages = m_stage;
			pipeline->m_pushConstants[resource.name].size = size;
			pipeline->m_pushConstants[resource.name].offset = offset;
		}
		
	}

	LOG_TRACE("-----UNIFORM BUFFERS-----");
	for(auto& resource : resources.uniform_buffers)
	{
		std::string name = comp.get_name(resource.id);
		name = name != "" ? name : resource.name;

		
		spirv_cross::SPIRType type = comp.get_type(resource.type_id);
		uint32_t set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
	    uint32_t binding = comp.get_decoration(resource.id, spv::DecorationBinding);
		
		uint32_t size = comp.get_declared_struct_size(type);
		uint32_t arraySize = type.array[0];

		if(type.array.size() > 1)
			LOG_WARN("{0} is an array of {1} dimensions in shader: {2}", name, type.array.size(), filename);

		if(type.array.size() == 0)
		{
			LOG_WARN("{0} is an array of {1} dimensions in shader: {2}, setting count to 1", name, type.array.size(), filename);
			arraySize = 1;
		}

		
		
		LOG_TRACE(name);
		LOG_TRACE("   Size: {0}", size);
		LOG_TRACE("   Set: {0}", set);
		LOG_TRACE("   Binding: {0}", binding);
		LOG_TRACE("   Count: {0}", arraySize);
		
		auto it = pipeline->m_uniformBuffers.find(name);
		if(it != pipeline->m_uniformBuffers.end())
		{
			it->second.stages |= m_stage;
		}
		else
		{
			pipeline->m_uniformBuffers[name].stages = m_stage;
			pipeline->m_uniformBuffers[name].size = size;
			pipeline->m_uniformBuffers[name].binding = binding;
			pipeline->m_uniformBuffers[name].set = set;
			pipeline->m_uniformBuffers[name].count = arraySize;
		}

	}

	LOG_TRACE("-----IMAGE SAMPLERS-----");
	for(auto& resource : resources.sampled_images)
	{
		std::string name = comp.get_name(resource.id);
		name = name != "" ? name : resource.name;
		
		spirv_cross::SPIRType type = comp.get_type(resource.type_id);
	    uint32_t binding = comp.get_decoration(resource.id, spv::DecorationBinding);
		uint32_t set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
		uint32_t arraySize = type.array[0];

		if(type.array.size() > 1)
			LOG_WARN("{0} is an array of {1} dimensions in shader: {2}", name, type.array.size(), filename);

		if(type.array.size() == 0)
		{
			LOG_WARN("{0} is an array of {1} dimensions in shader: {2}, setting count to 1", name, type.array.size(), filename);
			arraySize = 1;
		}

		if(arraySize == 0)
		{
			LOG_WARN("{0} has a count of 0 setting it to 1, this is most likely because the array uses variable indexing", name);
			arraySize = 1;
		}

		if(arraySize != OBJECTS_PER_DESCRIPTOR_CHUNK)
			LOG_WARN("{0}'s count is different than OBJECTS_PER_DESCRIPTOR_CHUNK({1})", name, OBJECTS_PER_DESCRIPTOR_CHUNK);

		LOG_TRACE(name);
		LOG_TRACE("   Set: {0}", set);
		LOG_TRACE("   Binding: {0}", binding);
		LOG_TRACE("   Count: {0}", arraySize);

		auto it = pipeline->m_textures.find(name);
		if(it != pipeline->m_textures.end())
		{
			it->second.stages |= m_stage;
		}
		else
		{
			pipeline->m_textures[name].stages = m_stage;
			pipeline->m_textures[name].binding = binding;
			pipeline->m_textures[name].set = set;
			pipeline->m_textures[name].count = arraySize;
		}
	}

	LOG_TRACE("-----STORAGE BUFFERS-----");
	for(auto& resource : resources.storage_buffers)
	{
		std::string name = comp.get_name(resource.id);
		name = name != "" ? name : resource.name;
		
		spirv_cross::SPIRType type = comp.get_type(resource.type_id);
		uint32_t set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
	    uint32_t binding = comp.get_decoration(resource.id, spv::DecorationBinding);
		uint32_t size = comp.get_declared_struct_size(type);
		uint32_t arraySize = type.array[0];

		if(size == 0)
		{
			size = comp.get_declared_struct_size_runtime_array(type, 1);
			LOG_WARN("{0} is a dynamic array setting size as if it had 1 element in shader: {1}", name,  filename);
		}
		
		if(type.array.size() > 1)
			LOG_WARN("{0} is an array of {1} dimensions in shader: {2}", name, type.array.size(), filename);

		if(type.array.size() == 0)
		{
			LOG_WARN("{0} is an array of {1} dimensions in shader: {2}, setting count to 1", name, type.array.size(), filename);
			arraySize = 1;
		}

		LOG_TRACE(name);
		LOG_TRACE("   Size: {0}", size);
		LOG_TRACE("   Set: {0}", set);
		LOG_TRACE("   Binding: {0}", binding);
		LOG_TRACE("   Count: {0}", arraySize);

		auto it = pipeline->m_storageBuffers.find(name);
		if(it != pipeline->m_storageBuffers.end())
		{
			it->second.stages |= m_stage;
		}
		else
		{
			pipeline->m_storageBuffers[name].stages = m_stage;
			pipeline->m_storageBuffers[name].size = size;
			pipeline->m_storageBuffers[name].binding = binding;
			pipeline->m_storageBuffers[name].set = set;
			pipeline->m_storageBuffers[name].count = arraySize;
		}
	}
	
	LOG_TRACE("-----STORAGE IMAGES-----");
	for(auto& resource : resources.storage_images)
	{
		std::string name = comp.get_name(resource.id);
		name = name != "" ? name : resource.name;
		
		spirv_cross::SPIRType type = comp.get_type(resource.type_id);
	    uint32_t binding = comp.get_decoration(resource.id, spv::DecorationBinding);
		uint32_t set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
		uint32_t arraySize = type.array[0];

		if(type.array.size() > 1)
			LOG_WARN("{0} is an array of {1} dimensions in shader: {2}", name, type.array.size(), filename);

		if(type.array.size() == 0)
		{
			LOG_WARN("{0} is an array of {1} dimensions in shader: {2}, setting count to 1", name, type.array.size(), filename);
			arraySize = 1;
		}

		if(arraySize == 0)
		{
			LOG_WARN("{0} has a count of 0 setting it to 1, this is most likely because the array uses variable indexing", name);
			arraySize = 1;
		}

		if(arraySize != OBJECTS_PER_DESCRIPTOR_CHUNK)
			LOG_WARN("{0}'s count is different than OBJECTS_PER_DESCRIPTOR_CHUNK({1})", name, OBJECTS_PER_DESCRIPTOR_CHUNK);

		LOG_TRACE(name);
		LOG_TRACE("   Set: {0}", set);
		LOG_TRACE("   Binding: {0}", binding);
		LOG_TRACE("   Count: {0}", arraySize);
		auto it = pipeline->m_storageImages.find(name);
		if(it != pipeline->m_storageImages.end())
		{
			it->second.stages |= m_stage;
		}
		else
		{
			pipeline->m_storageImages[name].stages = m_stage;
			pipeline->m_storageImages[name].binding = binding;
			pipeline->m_storageImages[name].set = set;
			pipeline->m_storageImages[name].count = arraySize;
		}
	}
}

