#include "Rendering/MaterialSystem.hpp"

#include <vulkan/vulkan.h>

#include "Pipeline.hpp"
#include "Rendering/Renderer.hpp"
#include "TextureManager.hpp"
#include "Application.hpp"
#include "Core/Events/EventHandler.hpp"

MaterialSystem::MaterialSystem()
    : m_renderer(Application::GetInstance()->GetRenderer()),
      m_ecs(Application::GetInstance()->GetScene()->GetECS())
{
    Application::GetInstance()->GetEventHandler()->Subscribe(this, &MaterialSystem::OnMaterialComponentAdded);
    Application::GetInstance()->GetEventHandler()->Subscribe(this, &MaterialSystem::OnMaterialComponentRemoved);
}

MaterialSystem::~MaterialSystem()
{
    for(auto& [name, sampler] : m_samplers)
    {
        vkDestroySampler(VulkanContext::GetDevice(), sampler, nullptr);
    }
}

void MaterialSystem::OnSceneSwitched(SceneSwitchedEvent e)
{
    m_ecs = e.newScene->GetECS();
}


void MaterialSystem::OnMaterialComponentAdded(ComponentAdded<Material> e)
{
    Material* comp = e.entity.GetComponentMut<Material>();
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
    auto materialIt = m_materialDatas.try_emplace(comp->shaderName, 500, sizeof(ShaderMaterial), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 50);
    uint64_t slot   = materialIt.first->second.Allocate(1, nullptr);
    comp->_ubSlot   = slot;  // TODO

    if(materialIt.second)
        VK_SET_DEBUG_NAME(materialIt.first->second.GetVkBuffer(), VK_OBJECT_TYPE_BUFFER, std::format("{}_materialBuffer", comp->shaderName).c_str());

    UpdateMaterial(comp);

    // add transform ptr + material info ptr to the pipeline's object data buffer
    if(pipeline->m_materialBufferPtr == 0)
        pipeline->m_materialBufferPtr = materialIt.first->second.GetDeviceAddress(0);
    // set object id in the material component for the renderer to be able to access it in the draw command creation
    /*
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
        });*/
}

void MaterialSystem::OnMaterialComponentRemoved(ComponentRemoved<Material> e)
{
    Material* comp         = e.entity.GetComponentMut<Material>();
    std::string shaderName = comp->shaderName;

    m_registry[shaderName]--;

    Pipeline* pipeline = m_renderer->m_pipelinesRegistry[shaderName];

    uint32_t numSwapchainImages = m_renderer->m_swapchainImages.size();


    m_freeTextureSlots[shaderName].push(comp->_textureSlot);
}

void MaterialSystem::UpdateMaterial(Material* material)
{
    auto materialIt = m_materialDatas.find(material->shaderName);
    if(materialIt == m_materialDatas.end())
    {
        LOG_ERROR("Trying to update material with name {} which does not exist", material->shaderName);
        return;
    }

    ShaderMaterial mat{};
    auto textureIt = material->textures.find("albedo");
    if(textureIt != material->textures.end())
    {
        Texture& albedo = TextureManager::GetTexture(textureIt->second);
        m_renderer->AddTexture(&albedo);
        mat.albedoTexture = albedo.GetSlot();
    }

    textureIt = material->textures.find("normal");
    if(textureIt != material->textures.end())
    {
        Texture& normal = TextureManager::GetTexture(textureIt->second);
        m_renderer->AddTexture(&normal);
        mat.normalTexture = normal.GetSlot();
    }

    textureIt = material->textures.find("roughness");
    if(textureIt != material->textures.end())
    {
        Texture& roughness = TextureManager::GetTexture(textureIt->second);
        m_renderer->AddTexture(&roughness);
        mat.roughnessTexture = roughness.GetSlot();
    }

    textureIt = material->textures.find("metallic");
    if(textureIt != material->textures.end())
    {
        Texture& metallic = TextureManager::GetTexture(textureIt->second);
        m_renderer->AddTexture(&metallic);
        mat.metallicTexture = metallic.GetSlot();
    }

    mat.albedo    = material->albedo;
    mat.roughness = material->roughness;
    mat.metallic  = material->metallic;


    materialIt->second.UploadData(material->_ubSlot, &mat);
}
