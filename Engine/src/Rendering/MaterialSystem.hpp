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

private:
    std::unordered_map<std::string, uint32_t> m_registry;
    std::unordered_map<std::string, VkSampler> m_samplers;                     // key is materialName + name of sampler
    std::unordered_map<std::string, std::queue<uint32_t>> m_freeTextureSlots;  // key is materialName
    std::unordered_map<std::string, DynamicBufferAllocator> m_materialDatas;
    Renderer* m_renderer;
    ECS* m_ecs;
};
