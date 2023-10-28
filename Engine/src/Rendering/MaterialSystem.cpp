#include "Rendering/MaterialSystem.hpp"
#include "Pipeline.hpp"
#include "Rendering/Renderer.hpp"
#include "ECS/ComponentManager.hpp"
#include "TextureManager.hpp"
#include "vulkan/vulkan_core.h"

MaterialSystem::MaterialSystem(Scene* scene, Renderer* renderer)
    : m_renderer(renderer),
      m_ecs(scene->ecs)
{
    scene->eventHandler->Subscribe(this, &MaterialSystem::OnMaterialComponentAdded);
    scene->eventHandler->Subscribe(this, &MaterialSystem::OnMaterialComponentRemoved);
}

MaterialSystem::~MaterialSystem()
{
    for(auto& [name, sampler] : m_samplers)
    {
        vkDestroySampler(VulkanContext::GetDevice(), sampler, nullptr);
    }
}

void MaterialSystem::OnMaterialComponentAdded(const ComponentAdded<Material>* e)
{
    Material* comp = e->component;
    auto it        = m_registry.find(comp->shaderName);

    Pipeline* pipeline = nullptr;

    if(it == m_registry.end())
    {
        PipelineCreateInfo ci;
        ci.type             = PipelineType::GRAPHICS;
        ci.allowDerivatives = true;
        ci.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
        ci.depthWriteEnable = false;
        ci.useDepth         = true;
        ci.stages           = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        ci.useMultiSampling = true;
        ci.useColorBlend    = true;

        pipeline = m_renderer->AddPipeline(comp->shaderName, ci, comp->priority);

        m_registry[comp->shaderName] = 1;
    }
    else
    {
        it->second += 1;
        pipeline    = m_renderer->m_pipelinesRegistry[comp->shaderName];
    }


    // add material info to pipeline's material buffer
    auto materialIt = m_materialDatas.try_emplace(comp->shaderName, 50, sizeof(Material), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0, true);
    bool didResize  = false;
    uint64_t slot   = materialIt.first->second.Allocate(1, didResize, nullptr);
    comp->_ubSlot   = slot;  // TODO

    // add transform ptr + material info ptr to the pipeline's object data buffer
    // set object id in the material component for the renderer to be able to access it in the draw command creation

    m_ecs->componentManager->Sort<Material>(
        [&](Material* lhs, Material* rhs)
        {
            if(lhs->shaderName == rhs->shaderName)
            {
                return lhs->_ubSlot < rhs->_ubSlot;
            }
            else
            {
                return *m_renderer->m_pipelinesRegistry[lhs->shaderName] < *m_renderer->m_pipelinesRegistry[rhs->shaderName];
            }
        });
    AllocateDescriptorSets(pipeline, e->component);
}

void MaterialSystem::OnMaterialComponentRemoved(const ComponentRemoved<Material>* e)
{
    Material* comp         = e->component;
    std::string shaderName = comp->shaderName;

    m_registry[shaderName]--;

    Pipeline* pipeline = m_renderer->m_pipelinesRegistry[shaderName];

    uint32_t numSwapchainImages = m_renderer->m_swapchainImages.size();

    for(int i = 0; i < numSwapchainImages; ++i)
    {
        for(auto& [name, bufferInfo] : pipeline->m_uniformBuffers)
        {
            if(bufferInfo.set != materialDescriptorSetIndex)
                continue;
            //m_renderer->m_ubAllocators[shaderName + name + std::to_string(i)]->Free(comp->_ubSlot);
        }
    }
    m_freeTextureSlots[shaderName].push(comp->_textureSlot);
}

void MaterialSystem::AllocateDescriptorSets(Pipeline* pipeline, Material* comp)
{
    /*
    uint32_t numSwapchainImages = m_renderer->m_swapchainImages.size();

    bool needToAllocate = m_registry[pipeline->m_name] % OBJECTS_PER_DESCRIPTOR_CHUNK == 1 || m_registry[pipeline->m_name] == 1;

    if(needToAllocate)
    {
        VkDescriptorSetAllocateInfo tallocInfo = {};
        std::vector<VkDescriptorSetLayout> tlayouts;

        for(int i = 0; i < numSwapchainImages; ++i)
        {
            tlayouts.push_back(pipeline->m_descSetLayouts[0]);
        }
        tallocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        tallocInfo.descriptorPool     = m_renderer->m_descriptorPool;
        tallocInfo.descriptorSetCount = static_cast<uint32_t>(tlayouts.size());
        tallocInfo.pSetLayouts        = tlayouts.data();
        m_renderer->m_tempDesc.resize(tlayouts.size());
        VK_CHECK(vkAllocateDescriptorSets(VulkanContext::GetDevice(), &tallocInfo, m_renderer->m_tempDesc.data()), "Failed to allocate descriptor sets");


        VkDescriptorSetAllocateInfo allocInfo = {};
        std::vector<VkDescriptorSetLayout> layouts(numSwapchainImages);

        uint32_t prevDescriptorSetCount = m_renderer->m_descriptorSets[pipeline->m_name + std::to_string(materialDescriptorSetIndex)].size();
        for(int i = 0; i < numSwapchainImages; ++i)
        {
            layouts[i] = pipeline->m_descSetLayouts[materialDescriptorSetIndex];

            m_renderer->m_descriptorSets[pipeline->m_name + std::to_string(materialDescriptorSetIndex)].push_back(VK_NULL_HANDLE);
        }
        allocInfo                    = {};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_renderer->m_descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts        = layouts.data();

        VkDescriptorSet* newDescriptorArray = &m_renderer->m_descriptorSets[pipeline->m_name + std::to_string(materialDescriptorSetIndex)][prevDescriptorSetCount];


        VK_CHECK(vkAllocateDescriptorSets(VulkanContext::GetDevice(), &allocInfo, newDescriptorArray), "Failed to allocate descriptor sets");
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

    for(int i = 0; i < numSwapchainImages; ++i)
    {
        VkDescriptorBufferInfo cameraBI = {};
        cameraBI.offset                 = 0;
        cameraBI.range                  = m_renderer->m_ubAllocators["camera" + std::to_string(i)]->GetObjSize();
        cameraBI.buffer                 = m_renderer->m_ubAllocators["camera" + std::to_string(i)]->GetBuffer(0);

        VkDescriptorBufferInfo lightsBI = {};
        lightsBI.offset                 = 0;
        lightsBI.range                  = VK_WHOLE_SIZE;
        lightsBI.buffer                 = m_renderer->m_lightsBuffers[i]->GetBuffer(0);

        VkDescriptorBufferInfo visibleLightsBI            = {};
        visibleLightsBI.offset                            = 0;
        visibleLightsBI.range                             = VK_WHOLE_SIZE;
        visibleLightsBI.buffer                            = m_renderer->m_visibleLightsBuffers[0]->GetBuffer(0);  // TODO index
        std::array<VkDescriptorBufferInfo, 2> bufferInfos = {lightsBI, visibleLightsBI};


        std::array<VkWriteDescriptorSet, 2> writeDS({});
        writeDS[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDS[0].dstSet          = m_renderer->m_tempDesc[i];
        writeDS[0].dstBinding      = 0;
        writeDS[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        writeDS[0].descriptorCount = 1;
        writeDS[0].pBufferInfo     = &cameraBI;

        writeDS[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDS[1].dstSet          = m_renderer->m_tempDesc[i];
        writeDS[1].dstBinding      = 1;
        writeDS[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeDS[1].descriptorCount = bufferInfos.size();
        writeDS[1].pBufferInfo     = bufferInfos.data();


        vkUpdateDescriptorSets(VulkanContext::GetDevice(), writeDS.size(), writeDS.data(), 0, nullptr);

        for(auto& [name, bufferInfo] : pipeline->m_uniformBuffers)
        {
            if(bufferInfo.set != materialDescriptorSetIndex)
                continue;

            uint32_t ubSlot = m_renderer->m_ubAllocators[pipeline->m_name + name + std::to_string(i)]->Allocate();
            comp->_ubSlot   = ubSlot;

            if(needToAllocate)
            {
                VkDescriptorBufferInfo bufferI = {};
                bufferI.buffer                 = m_renderer->m_ubAllocators[pipeline->m_name + name + std::to_string(i)]->GetBuffer(ubSlot);
                bufferI.offset                 = 0;
                bufferI.range                  = bufferInfo.size;

                VkWriteDescriptorSet writeDS = {};
                writeDS.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                uint32_t descriptorIndex     = (ubSlot / OBJECTS_PER_DESCRIPTOR_CHUNK) * numSwapchainImages + i;
                writeDS.dstSet               = m_renderer->m_descriptorSets[pipeline->m_name + "1"][descriptorIndex];
                writeDS.dstBinding           = bufferInfo.binding;
                writeDS.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                writeDS.descriptorCount      = 1;
                writeDS.pBufferInfo          = &bufferI;

                vkUpdateDescriptorSets(VulkanContext::GetDevice(), 1, &writeDS, 0, nullptr);
            }
        }
        // we always need to call vkUpdateDescriptorSets for textures, since we are only using the first N texture samplers in the array

        for(auto& [name, textureInfo] : pipeline->m_textures)
        {
            if(textureInfo.set != materialDescriptorSetIndex)
                continue;

            std::string texturePath = comp->textures[name];
            Texture& texture        = TextureManager::GetTexture(texturePath);

            auto it = m_samplers.find(pipeline->m_name + name);

            if(it == m_samplers.end())
            {
                m_samplers[pipeline->m_name + name] = VK_NULL_HANDLE;

                VkSamplerCreateInfo ci     = {};
                ci.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                ci.magFilter               = VK_FILTER_LINEAR;
                ci.minFilter               = VK_FILTER_LINEAR;
                ci.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                ci.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                ci.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                ci.anisotropyEnable        = VK_TRUE;
                ci.maxAnisotropy           = 16;
                ci.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
                ci.unnormalizedCoordinates = VK_FALSE;  // this should always be false because UV coords are in [0,1) not in [0, width),etc...
                ci.compareEnable           = VK_FALSE;  // this is used for percentage closer filtering for shadow maps
                ci.compareOp               = VK_COMPARE_OP_ALWAYS;
                ci.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                ci.mipLodBias              = 0.0f;
                ci.minLod                  = 0.0f;
                ci.maxLod                  = texture.GetMipLevels();

                VK_CHECK(vkCreateSampler(VulkanContext::GetDevice(), &ci, nullptr, &m_samplers[pipeline->m_name + name]), "Failed to create texture sampler");
            }


            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.sampler               = m_samplers[pipeline->m_name + name];
            imageInfo.imageView             = texture.GetImageView();  // texture.GetImageView();

            VkWriteDescriptorSet writeDS = {};
            writeDS.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDS.dstBinding           = textureInfo.binding;
            writeDS.descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDS.dstArrayElement      = textureSlot % OBJECTS_PER_DESCRIPTOR_CHUNK;

            uint32_t descriptorIndex = (textureSlot / OBJECTS_PER_DESCRIPTOR_CHUNK) * numSwapchainImages + i;
            writeDS.dstSet           = m_renderer->m_descriptorSets[pipeline->m_name + "1"][descriptorIndex];

            writeDS.descriptorCount = 1;
            writeDS.pImageInfo      = &imageInfo;

            vkUpdateDescriptorSets(VulkanContext::GetDevice(), 1, &writeDS, 0, nullptr);
        }
    }
*/
}


void MaterialSystem::UpdateMaterial(Material* mat)
{
    /*
    Pipeline* pipeline          = m_renderer->m_pipelinesRegistry[mat->shaderName];
    uint32_t numSwapchainImages = m_renderer->m_swapchainImages.size();

    for(int i = 0; i < numSwapchainImages; ++i)
    {
        for(auto& [name, textureInfo] : pipeline->m_textures)
        {
            if(textureInfo.set != materialDescriptorSetIndex)
                continue;

            std::string texturePath = mat->textures[name];
            Texture& texture        = TextureManager::GetTexture(texturePath);

            auto it = m_samplers.find(pipeline->m_name + name);


            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageLayout           = texture.GetLayout();
            imageInfo.sampler               = m_samplers[pipeline->m_name + name];
            imageInfo.imageView             = texture.GetImageView();

            VkWriteDescriptorSet writeDS = {};
            writeDS.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDS.dstBinding           = textureInfo.binding;
            writeDS.descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDS.dstArrayElement      = mat->_textureSlot % OBJECTS_PER_DESCRIPTOR_CHUNK;

            uint32_t descriptorIndex = (mat->_textureSlot / OBJECTS_PER_DESCRIPTOR_CHUNK) * numSwapchainImages + i;
            writeDS.dstSet           = m_renderer->m_descriptorSets[pipeline->m_name + "1"][descriptorIndex];

            writeDS.descriptorCount = 1;
            writeDS.pImageInfo      = &imageInfo;

            vkUpdateDescriptorSets(VulkanContext::GetDevice(), 1, &writeDS, 0, nullptr);  // TODO this may need better synchro in the future, but for now update_unused_whil_pending is enough
        }
    }
    */
}
