#pragma once

#include <unordered_map>
#include <vector>

#include "ECS/Entity.hpp"
#include "ECS/Types.hpp"
#include "Memory/MemoryChunkAllocator.hpp"
#include "ECS/EventHandler.hpp"
#include "ECS/CoreEvents/EntityEvents.hpp"
#include "ECS/ECSEngine.hpp"
#include "ECS/CoreComponents/Relationship.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "ECS/CoreComponents/NameTag.hpp"

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
	virtual ~EntityManager() override = default;;

	template<typename... Args>
	EntityID CreateEntity(Args... args)
	{
		void* pMemory = this->CreateObject();

		((Entity*)pMemory)->m_id = m_lastID++;
		((Entity*)pMemory)->m_componentManager = m_ecsEngine->componentManager;

		Entity* entity = new(pMemory) Entity(std::forward<Args>(args)...);

		NameTag* tag = m_ecsEngine->componentManager->AddComponent<NameTag>(entity->GetEntityID());
		tag->name = "Entity" + std::to_string(entity->GetEntityID());
		
		Transform* transform = m_ecsEngine->componentManager->AddComponent<Transform>(entity->GetEntityID());
		
		Relationship* comp = m_ecsEngine->componentManager->AddComponent<Relationship>(entity->GetEntityID());
		comp->parent = INVALID_ENTITY_ID;
		// TODO: maybe make entities have transform component by default too


		EntityCreated e;
		e.entity = *entity;
		m_ecsEngine->eventHandler->Send<EntityCreated>(e);

		return entity->GetEntityID();
	}

	template<typename... Args>
	EntityID CreateChild(EntityID parent, Args... args)
	{
		EntityID entity = CreateEntity(std::forward<Args>(args)...);

		Relationship* relationshipComp = m_ecsEngine->componentManager->GetComponent<Relationship>(entity);

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

	std::vector<Entity*> GetAllEntities() const
	{
		std::vector<Entity*> result;
		result.reserve(m_handleTable.size());
		
		for (auto& [id, entity] : m_handleTable)
		{
			result.push_back(entity);
		}

		return result;
	}
};