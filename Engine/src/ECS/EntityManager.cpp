#include "ECS/EntityManager.hpp"
#include "ECS/ComponentManager.hpp"

EntityManager::EntityManager(ComponentManager* componentManager):
	m_numPendingDestroy(0),
	m_pendingDestroy(128),
	m_componentManager(componentManager),
	m_lastID(0) {}

EntityManager::~EntityManager()
{
	for (auto container : m_registry)
	{
		delete container.second;
		container.second = nullptr;
	}
}

void EntityManager::DestroyEntity(EntityID id)
{


	if(m_numPendingDestroy < m_pendingDestroy.size())
		m_pendingDestroy[m_numPendingDestroy++] = id;
	else
	{
		m_pendingDestroy.push_back(id);
		m_numPendingDestroy++;
	}
}

void EntityManager::RemoveDestroyedEntities()
{
	for(size_t i = 0; i < m_numPendingDestroy; ++i)
	{
		EntityID id = m_pendingDestroy[i];
		IEntity* entity = m_handleTable[id];

		EntityTypeID typeId = entity->GetTypeID();

		auto it = m_registry.find(typeId);
		if(it != m_registry.end())
		{
			m_componentManager->RemoveAllComponents(id);

			it->second->DestroyEntity(entity);
		}
	}
	m_numPendingDestroy = 0;
}
