#pragma once

#include "ECS/System.hpp"
#include "ECS/CoreEvents/ComponentEvents.hpp"
#include "ECS/CoreComponents/Material.hpp"
#include "ECS/CoreEvents/SystemEvents.hpp"
#include <queue>

const uint32_t materialDescriptorSetIndex = 1;

class Pipeline;
class RendererSystem;
class MaterialSystem : public System<MaterialSystem>
{
public:
	MaterialSystem();
	~MaterialSystem();

	void UpdateMaterial(Material* mat);
private:

	void OnMaterialComponentAdded(const ComponentAdded<Material>* e);
	void OnRendererSystemAdded(const SystemAdded<RendererSystem>* e);
	void OnMaterialComponentRemoved(const ComponentRemoved<Material>* e);

	void AllocateDescriptorSets(Pipeline* pipeline, Material* comp);

	std::unordered_map<std::string, uint32_t> m_registry;
	std::unordered_map<std::string, VkSampler> m_samplers; // key is materialName + name of sampler
	std::unordered_map<std::string, std::queue<uint32_t>> m_freeTextureSlots; // key is materialName
	RendererSystem* m_rendererSystem;

};
