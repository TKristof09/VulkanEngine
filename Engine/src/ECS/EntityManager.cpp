#include "ECS/EntityManager.hpp"
#include "ECS/ComponentManager.hpp"

EntityManager::EntityManager(ECSEngine* ecsEngine):
	m_numPendingDestroy(0),
	m_pendingDestroy(128),
	m_ecsEngine(ecsEngine),
	m_lastID(0) {}


void EntityManager::DestroyEntity(EntityID id)
{

	EntityDestroyed e;
	e.entity = *m_handleTable[id];
	m_ecsEngine->eventHandler->Send<EntityDestroyed>(e);
	m_ecsEngine->componentManager->RemoveAllComponents(id);
	
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
		Entity* entity = m_handleTable[id];

		FreeObject(entity);
	}
	
	m_numPendingDestroy = 0;
}
