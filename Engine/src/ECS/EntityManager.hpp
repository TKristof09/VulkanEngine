#pragma once

#include <unordered_map>
#include <vector>

#include "Core/Events/EventHandler.hpp"
#include "Core/Scene/Scene.hpp"
#include "ECS/ECSEngine.hpp"
#include "ECS/Entity.hpp"
#include "ECS/Types.hpp"
#include "Memory/MemoryChunkAllocator.hpp"
#include "ECS/CoreEvents/EntityEvents.hpp"
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

	Scene m_scene;


	uint64_t m_lastID;

public:
	EntityManager(Scene scene);
	virtual ~EntityManager() override
	{
		for (auto [id, entity] : m_handleTable)
		{
			DestroyEntity(id);
		}
		RemoveDestroyedEntities();
	}

	template<typename... Args>
	Entity* CreateEntity(Args... args)
	{
		void* pMemory = this->CreateObject();

		((Entity*)pMemory)->m_id = m_lastID++;
		((Entity*)pMemory)->m_componentManager = m_scene.ecs->componentManager;

		Entity* entity = new(pMemory) Entity(std::forward<Args>(args)...);

		NameTag* tag = entity->AddComponent<NameTag>();
		tag->name = "Entity" + std::to_string(entity->GetEntityID());
		
		Transform* transform = entity->AddComponent<Transform>();
		
		Relationship* comp = entity->AddComponent<Relationship>();
		comp->parent = INVALID_ENTITY_ID;
		// TODO: maybe make entities have transform component by default too


		EntityCreated e;
		e.entity = *entity;
		m_scene.eventHandler->Send<EntityCreated>(e);

		m_handleTable[entity->GetEntityID()] = entity;

		return entity;
	}

	template<typename... Args>
	Entity* CreateChild(Entity* parent, Args... args)
	{
		Entity* entity = CreateEntity(std::forward<Args>(args)...);

		Relationship* relationshipComp = entity->GetComponent<Relationship>();

		Relationship* relationshipParent = parent->GetComponent<Relationship>();
		relationshipParent->numChildren++;

		relationshipComp->parent = parent->GetEntityID();

		EntityID prev = relationshipParent->firstChild;
		relationshipParent->firstChild = entity->GetEntityID();
		
		if(prev != INVALID_ENTITY_ID)
		{
			m_scene.ecs->componentManager->GetComponent<Relationship>(prev)->previousSibling = entity->GetEntityID();
			relationshipComp->nextSibling = prev;
		}
		

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
