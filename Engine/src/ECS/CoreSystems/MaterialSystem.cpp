#include "ECS/CoreSystems/MaterialSystem.hpp"
#include "Pipeline.hpp"
#include "ECS/CoreSystems/RendererSystem.hpp"
#include "ECS/ComponentManager.hpp"
#include <unistd.h>

MaterialSystem::MaterialSystem()
{
	Subscribe(&MaterialSystem::OnRendererSystemAdded);
	Subscribe(&MaterialSystem::OnMaterialComponentAdded);
	Subscribe(&MaterialSystem::OnMaterialComponentRemoved);
}

MaterialSystem::~MaterialSystem()
{
	for(auto& [name, sampler] : m_samplers)
	{
		vkDestroySampler(VulkanContext::GetDevice(), sampler, nullptr);
	}
}

void MaterialSystem::OnRendererSystemAdded(const SystemAdded<RendererSystem>* e)
{
	m_rendererSystem = e->ptr;

	m_errorTexture = Texture("./textures/error.jpg");
	VkSamplerCreateInfo ci = {};
	ci.sType	= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	ci.magFilter = VK_FILTER_LINEAR;
	ci.minFilter = VK_FILTER_LINEAR;
	ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	ci.anisotropyEnable = VK_TRUE;
	ci.maxAnisotropy = 16;
	ci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	ci.unnormalizedCoordinates = VK_FALSE; // this should always be false because UV coords are in [0,1) not in [0, width),etc...
	ci.compareEnable = VK_FALSE; // this is used for percentage closer filtering for shadow maps
	ci.compareOp     = VK_COMPARE_OP_ALWAYS;
	ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	ci.mipLodBias = 0.0f;
	ci.minLod = 0.0f;
	ci.maxLod = m_errorTexture.GetMipLevels();

	VK_CHECK(vkCreateSampler(VulkanContext::GetDevice(), &ci, nullptr, &m_errorSampler), "Failed to create texture sampler");
}
void MaterialSystem::OnMaterialComponentAdded(const ComponentAdded<Material>* e)
{
	Material* comp = e->component;
	auto it = m_registry.find(comp->shaderName);

	Pipeline* pipeline = nullptr;

	if(it == m_registry.end())
	{
		PipelineCreateInfo ci;
		ci.allowDerivatives	= true;
		ci.depthCompareOp		= VK_COMPARE_OP_LESS_OR_EQUAL; //maybe it should even be EQUAL since we only need to render the fragments that are in the depth image
		ci.depthWriteEnable	= false;
		ci.useDepth			= true;
		ci.subpass			= 0;
		ci.stages			= (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		ci.useMultiSampling	= true;
		ci.useColorBlend	= true;
		ci.useVariableDescriptorCount	= true;

		pipeline = m_rendererSystem->AddPipeline(comp->shaderName, ci, comp->priority);

		m_registry[comp->shaderName] = 1;
	}
	else
	{
		it->second += 1;
		pipeline = m_rendererSystem->m_pipelinesRegistry[comp->shaderName];
	}


	m_ecsEngine->componentManager->Sort<Material>([&](Material* lhs, Material* rhs)
	{
		return *m_rendererSystem->m_pipelinesRegistry[lhs->shaderName] < *m_rendererSystem->m_pipelinesRegistry[rhs->shaderName];
	});

	AllocateDescriptorSets(pipeline, e->component);
}

void MaterialSystem::OnMaterialComponentRemoved(const ComponentRemoved<Material>* e)
{
	Material* comp = e->component;
	std::string shaderName = comp->shaderName;

	m_registry[shaderName]--;

	Pipeline* pipeline = m_rendererSystem->m_pipelinesRegistry[shaderName];

	uint32_t numSwapchainImages = m_rendererSystem->m_swapchainImages.size();

	for (int i = 0; i < numSwapchainImages; ++i)
	{
		for(auto& shader : pipeline->m_shaders)
		{
			for(auto& [name, bufferInfo] : shader.m_uniformBuffers)
			{
				if(bufferInfo.set != materialDescriptorSetIndex)
					continue;
				m_rendererSystem->m_ubAllocators[shaderName + name + std::to_string(i)]->Free(comp->_ubSlot);
			}
		}
	}
	m_freeTextureSlots[shaderName].push(comp->_textureSlot);

}

void MaterialSystem::AllocateDescriptorSets(Pipeline* pipeline, Material* comp)
{


	uint32_t numSwapchainImages = m_rendererSystem->m_swapchainImages.size();

	bool needToAllocate = m_registry[pipeline->m_name] > OBJECTS_PER_DESCRIPTOR_CHUNK || m_registry[pipeline->m_name] == 1;

	if(needToAllocate)
	{

		VkDescriptorSetAllocateInfo allocInfo = {};
		std::vector<VkDescriptorSetLayout> layouts(numSwapchainImages);

		for (int i = 0; i < numSwapchainImages; ++i)
		{
			layouts[i] = pipeline->m_descSetLayouts[materialDescriptorSetIndex];
		}
		allocInfo = {};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool		= m_rendererSystem->m_descriptorPool;
		allocInfo.descriptorSetCount	= static_cast<uint32_t>(layouts.size());
		allocInfo.pSetLayouts			= layouts.data();

		std::vector<uint32_t> counts(layouts.size(), OBJECTS_PER_DESCRIPTOR_CHUNK);
		if(!comp->textures.empty()) // TODO for now we use it every time there is a texture
		{

			VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountAlloc = {};
			variableCountAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
			variableCountAlloc.descriptorSetCount = static_cast<uint32_t>(layouts.size());
			variableCountAlloc.pDescriptorCounts = counts.data();

			allocInfo.pNext = &variableCountAlloc;
		}
		m_rendererSystem->m_descriptorSets[pipeline->m_name + std::to_string(materialDescriptorSetIndex)].resize(numSwapchainImages);
		VK_CHECK(vkAllocateDescriptorSets(VulkanContext::GetDevice(), &allocInfo, m_rendererSystem->m_descriptorSets[pipeline->m_name + std::to_string(materialDescriptorSetIndex)].data()), "Failed to allocate descriptor sets");


#if 0 // this is only needed if we want to bind "error" textures into every slot in order to detect an incorrect texture usage, not sure if i want this because validation layers scream at me anyway
		for (int i = 0; i < numSwapchainImages; ++i)
		{
			for(auto& shader : pipeline->m_shaders)
			{
				for(auto& [name, textureInfo] : shader.m_textures)
				{
					if(textureInfo.set != materialDescriptorSetIndex)
						continue;

					std::vector<VkDescriptorImageInfo> imageInfos(counts[0]);
					for(uint32_t i=0; i < counts[0]; ++i)
					{
						imageInfos[i].imageLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						imageInfos[i].sampler		= m_errorSampler;
						imageInfos[i].imageView		= m_errorTexture.GetImageView();
					}
					VkWriteDescriptorSet writeDS = {};
					writeDS.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writeDS.dstBinding		= textureInfo.binding;
					writeDS.descriptorType	= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					writeDS.dstArrayElement = 0;
					writeDS.dstSet			= m_rendererSystem->m_descriptorSets[pipeline->m_name + "1"][i];

					writeDS.descriptorCount = imageInfos.size();
					writeDS.pImageInfo		= imageInfos.data();

					//vkUpdateDescriptorSets(VulkanContext::GetDevice(), 1, &writeDS, 0, nullptr);
					LOG_TRACE("Written to set {0}, binding {1} at index {2}", textureInfo.set, writeDS.dstBinding, writeDS.dstArrayElement );

				}
			}
		}
#endif
	}

	// texture slot in the big arrays of textures is the same for every texture of the material, no matter which sampler it is, and no matter the index of swapchain image
	// same for the ub slot in each of the big dynamic arrays
	uint32_t textureSlot;
	if(!m_freeTextureSlots[pipeline->m_name].empty())
	{
		textureSlot = m_freeTextureSlots[pipeline->m_name].front();
		m_freeTextureSlots[pipeline->m_name].pop();
	}
	else
	{
		textureSlot = m_registry[pipeline->m_name] - 1;
	}
	comp->_textureSlot = textureSlot;

	for (int i = 0; i < numSwapchainImages; ++i)
	{
		for(auto& shader : pipeline->m_shaders)
		{
			for(auto& [name, bufferInfo] : shader.m_uniformBuffers)
			{
				if(bufferInfo.set != materialDescriptorSetIndex)
					continue;

				uint32_t ubSlot = m_rendererSystem->m_ubAllocators[pipeline->m_name + name + std::to_string(i)]->Allocate();
				comp->_ubSlot = ubSlot;

				if(needToAllocate)
				{
					VkDescriptorBufferInfo bufferI = {};
					bufferI.buffer	= m_rendererSystem->m_ubAllocators[pipeline->m_name + name + std::to_string(i)]->GetBuffer(ubSlot);
					bufferI.offset	= 0;
					bufferI.range	= bufferInfo.size;

					VkWriteDescriptorSet writeDS = {};
					writeDS.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writeDS.dstSet			= m_rendererSystem->m_descriptorSets[pipeline->m_name + "1"][i];
					writeDS.dstBinding		= bufferInfo.binding;
					writeDS.descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
					writeDS.descriptorCount	= 1;
					writeDS.pBufferInfo		= &bufferI;

					vkUpdateDescriptorSets(VulkanContext::GetDevice(), 1, &writeDS, 0, nullptr);
				}
			}
			// we always need to call vkUpdateDescriptorSets for textures, since we are only using the first N texture samplers in the array

			for(auto& [name, textureInfo] : shader.m_textures)
			{
				if(textureInfo.set != materialDescriptorSetIndex)
					continue;

				Texture& texture = comp->textures[name];

				auto it = m_samplers.find(pipeline->m_name + name);

				if(it == m_samplers.end())
				{
					m_samplers[pipeline->m_name + name] = VK_NULL_HANDLE;

					VkSamplerCreateInfo ci = {};
					ci.sType	= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
					ci.magFilter = VK_FILTER_LINEAR;
					ci.minFilter = VK_FILTER_LINEAR;
					ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					ci.anisotropyEnable = VK_TRUE;
					ci.maxAnisotropy = 16;
					ci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
					ci.unnormalizedCoordinates = VK_FALSE; // this should always be false because UV coords are in [0,1) not in [0, width),etc...
					ci.compareEnable = VK_FALSE; // this is used for percentage closer filtering for shadow maps
					ci.compareOp     = VK_COMPARE_OP_ALWAYS;
					ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
					ci.mipLodBias = 0.0f;
					ci.minLod = 0.0f;
					ci.maxLod = texture.GetMipLevels();

					VK_CHECK(vkCreateSampler(VulkanContext::GetDevice(), &ci, nullptr, &m_samplers[pipeline->m_name + name]), "Failed to create texture sampler");

				}


				VkDescriptorImageInfo imageInfo = {};
				imageInfo.imageLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.sampler		= m_samplers[pipeline->m_name + name];
				imageInfo.imageView		= texture.GetImageView();

				VkWriteDescriptorSet writeDS = {};
				writeDS.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDS.dstBinding		= textureInfo.binding;
				writeDS.descriptorType	= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writeDS.dstArrayElement = textureSlot;
				writeDS.dstSet			= m_rendererSystem->m_descriptorSets[pipeline->m_name + "1"][i];

				writeDS.descriptorCount = 1;
				writeDS.pImageInfo		= &imageInfo;

				vkUpdateDescriptorSets(VulkanContext::GetDevice(), 1, &writeDS, 0, nullptr);
			}

		}
	}
}

void MaterialSystem::BindMaterialInstance(Material* material)
{
#if 0
	Pipeline* pipeline = material->_pipeline;

	for(auto& shader : pipeline->m_shaders)
	{
		for(auto& [name, bufferInfo] : shader.m_uniformBuffers)
		{
			if(bufferInfo.set != materialDescriptorSetIndex)
				continue;


			if(needToAllocate)
			{
				VkDescriptorBufferInfo bufferI = {};
				bufferI.buffer	= m_rendererSystem->m_ubAllocators[pipeline->m_name + name + std::to_string(i)]->GetBuffer(ubSlot);
				bufferI.offset	= 0;
				bufferI.range	= VK_WHOLE_SIZE;

				VkWriteDescriptorSet writeDS = {};
				writeDS.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDS.dstSet			= m_rendererSystem->m_descriptorSets[pipeline->m_name + "1"][i];
				writeDS.dstBinding		= bufferInfo.binding;
				writeDS.descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				writeDS.descriptorCount	= 1;
				writeDS.pBufferInfo		= &bufferI;

				vkUpdateDescriptorSets(VulkanContext::GetDevice(), 1, &writeDS, 0, nullptr);
			}
		}

		for(auto& [name, textureInfo] : shader.m_textures)
		{
			if(textureInfo.set != materialDescriptorSetIndex)
				continue;

			Texture& texture = material->textures[name];

			auto it = m_samplers.find(pipeline->m_name + name);

			if(it == m_samplers.end())
			{
				m_samplers[pipeline->m_name + name] = VK_NULL_HANDLE;

				VkSamplerCreateInfo ci = {};
				ci.sType	= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
				ci.magFilter = VK_FILTER_LINEAR;
				ci.minFilter = VK_FILTER_LINEAR;
				ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				ci.anisotropyEnable = VK_TRUE;
				ci.maxAnisotropy = 16;
				ci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
				ci.unnormalizedCoordinates = VK_FALSE; // this should always be false because UV coords are in [0,1) not in [0, width),etc...
				ci.compareEnable = VK_FALSE; // this is used for percentage closer filtering for shadow maps
				ci.compareOp     = VK_COMPARE_OP_ALWAYS;
				ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				ci.mipLodBias = 0.0f;
				ci.minLod = 0.0f;
				ci.maxLod = texture.GetMipLevels();

				VK_CHECK(vkCreateSampler(VulkanContext::GetDevice(), &ci, nullptr, &m_samplers[pipeline->m_name + name]), "Failed to create texture sampler");

			}


			VkDescriptorImageInfo imageInfo = {};
			imageInfo.imageLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.sampler		= m_samplers[pipeline->m_name + name];
			imageInfo.imageView		= texture.GetImageView();

			VkWriteDescriptorSet writeDS = {};
			writeDS.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDS.dstBinding		= textureInfo.binding;
			writeDS.descriptorType	= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDS.dstArrayElement = 0;
			writeDS.dstSet			= m_rendererSystem->m_descriptorSets[pipeline->m_name + "1"][imageIndex];

			writeDS.descriptorCount = 1;
			writeDS.pImageInfo		= &imageInfo;

			vkUpdateDescriptorSets(VulkanContext::GetDevice(), 1, &writeDS, 0, nullptr);

		}

	}
#endif
}