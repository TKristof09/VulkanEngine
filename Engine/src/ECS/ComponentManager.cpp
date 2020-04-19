#include "ECS/ComponentManager.hpp"

ComponentManager::ComponentManager()
{}

ComponentManager::~ComponentManager()
{
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

		m_registry[it.first]->DestroyComponent(component);

		m_componentMap.erase(it.second);
	}
	m_entityComponentMap.erase(entityId);

}
