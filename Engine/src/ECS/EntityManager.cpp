#include "ECS/EntityManager.hpp"
#include "ECS/ComponentManager.hpp"

EntityManager::EntityManager(ECSEngine* ecsEngine):
	m_numPendingDestroy(0),
	m_pendingDestroy(128),
	m_ecsEngine(ecsEngine),
	m_lastID(0) {}

EntityManager::~EntityManager()
{}

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
		Entity* entity = m_handleTable[id];

		m_ecsEngine->m_componentManager->RemoveAllComponents(id);

		this->FreeObject(entity);
		EntityDestroyed e;
		e.entity = id;
		m_ecsEngine->m_eventHandler->Send<EntityDestroyed>(e);
	}
	m_numPendingDestroy = 0;
}
