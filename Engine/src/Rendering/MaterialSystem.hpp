#pragma once

#include "Core/Events/CoreEvents.hpp"
#include "ECS/System.hpp"
#include "ECS/CoreEvents/ComponentEvents.hpp"
#include "ECS/CoreComponents/Material.hpp"
#include "Buffer.hpp"
#include <queue>

const uint32_t materialDescriptorSetIndex = 1;

class Pipeline;
class Renderer;
class MaterialSystem
{
public:
    MaterialSystem();
    ~MaterialSystem();

    void OnMaterialComponentAdded(ComponentAdded<Material> e);
    void OnMaterialComponentRemoved(ComponentRemoved<Material> e);

    void OnSceneSwitched(SceneSwitchedEvent e);

    void UpdateMaterial(Material* material);

    uint64_t GetMaterialBufferAddress(const std::string& materialName)
    {
        auto it = m_materialDatas.find(materialName);
        if(it == m_materialDatas.end())
        {
            LOG_ERROR("Trying to get material buffer address for material {} which does not exist", materialName);
            return 0;
        }

        return it->second.GetDeviceAddress(0);
    }

private:
    std::unordered_map<std::string, uint32_t> m_registry;
    std::unordered_map<std::string, VkSampler> m_samplers;                     // key is materialName + name of sampler
    std::unordered_map<std::string, std::queue<uint32_t>> m_freeTextureSlots;  // key is materialName
    std::unordered_map<std::string, DynamicBufferAllocator> m_materialDatas;
    Renderer* m_renderer;
    ECS* m_ecs;
};
