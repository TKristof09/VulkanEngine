#pragma once

#include <unordered_map>
#include <vector>

#include "ECS/Entity.hpp"
#include "Memory/MemoryChunkAllocator.hpp"
#include "ECS/CoreComponents/Relationship.hpp"
#include "ECS/EventHandler.hpp"
#include "ECS/CoreEvents/EntityEvents.hpp"
#include "ECS/ECSEngine.hpp"

#define CHUNK_SIZE 512


class EntityManager : public MemoryChunkAllocator<Entity, CHUNK_SIZE>
{
private:
	EntityManager(const EntityManager&) = delete;
	EntityManager& operator=(EntityManager&) = delete;


	std::vector<EntityID> m_pendingDestroy;
	size_t m_numPendingDestroy;

	std::unordered_map<EntityID, Entity*> m_handleTable;

	ECSEngine* m_ecsEngine;


	uint64_t m_lastID;

public:
	EntityManager(ECSEngine* ecsEngine);
	~EntityManager();

	template<typename... Args>
	EntityID CreateEntity(Args... args)
	{
		void* pMemory = this->CreateObject();

		((Entity*)pMemory)->m_id = m_lastID++;
		((Entity*)pMemory)->m_componentManager = m_ecsEngine->componentManager;

		Entity* entity = new(pMemory) Entity(std::forward<Args>(args)...);

		m_ecsEngine->componentManager->AddComponent<Relationship>(entity->GetEntityID());
		// TODO: maybe make entities have transform component by default too

		EntityCreated e;
		e.entity = entity->GetEntityID();
		m_ecsEngine->eventHandler->Send<EntityCreated>(e);

		return entity->GetEntityID();
	}

	template<typename... Args>
	EntityID CreateChild(EntityID parent, Args... args)
	{
		EntityID entity = CreateEntity(std::forward<Args>(args)...);

		Relationship* relationshipComp = m_ecsEngine->componentManager->AddComponent<Relationship>(entity);

		Relationship* relationshipParent = m_ecsEngine->componentManager->GetComponent<Relationship>(parent);
		relationshipParent->numChildren++;

		relationshipComp->parent = parent;

		EntityID prev = m_ecsEngine->componentManager->GetComponent<Relationship>(parent)->firstChild;

		if(prev == INVALID_ENTITY_ID)
			relationshipParent->firstChild = entity;
		else
		{
			while(true)
			{
				EntityID sibling = m_ecsEngine->componentManager->GetComponent<Relationship>(prev)->nextSibling;
				if(sibling != INVALID_ENTITY_ID)
				{
					prev = sibling;
					break;
				}
			}

			relationshipComp->previousSibling = prev;
			m_ecsEngine->componentManager->GetComponent<Relationship>(prev)->nextSibling = entity;
		}

		// TODO: maybe make entities have transform component by default too

		return entity;

	}

	void DestroyEntity(EntityID id);
	void RemoveDestroyedEntities();

	Entity* GetEntity(EntityID id) { return m_handleTable[id]; }


};
