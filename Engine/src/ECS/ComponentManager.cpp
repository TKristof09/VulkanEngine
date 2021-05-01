#include "ECS/ComponentManager.hpp"

ComponentManager::ComponentManager(Scene scene):
	m_scene(scene)
{}

ComponentManager::~ComponentManager()
{
	DestroyRemovedComponents();
	for (auto container : m_registry)
	{
		delete container.second;
		container.second = nullptr;
	}
}

void ComponentManager::RemoveAllComponents(const EntityID entityId)
{
	size_t numComponents = m_entityComponentMap[entityId].size();
	for(auto& it : m_entityComponentMap[entityId])
	{
		IComponent* component = m_componentMap[it.second];

		m_componentsToRemove.push(component);

		m_componentMap.erase(it.second);

	}
	m_entityComponentMap.erase(entityId);

}

void ComponentManager::DestroyRemovedComponents()
{
	while(!m_componentsToRemove.empty())
	{
		IComponent* comp = m_componentsToRemove.front();
		
		m_registry[comp->m_typeID]->DestroyComponent(comp);

		m_componentsToRemove.pop();
	}
}
