#pragma once

#include "IEntity.hpp"
#include "Memory/MemoryChunkAllocator.hpp"
#include <unordered_map>

class ComponentManager;
#define CHUNK_SIZE 512

class EntityManager
{
	class IEntityContainer
	{
	public:
		virtual ~IEntityContainer() {};
		virtual void DestroyEntity(IEntity* entity) = 0;
	};
	template<typename T>
	class EntityContainer : public IEntityContainer, public MemoryChunkAllocator<T, CHUNK_SIZE>
	{
	public:
		EntityContainer() {}
		virtual ~EntityContainer() {}
		virtual void DestroyEntity(IEntity* entity) override
		{
			entity->~IEntity();
			this->DestroyObject(entity);
		}
	private:
		EntityContainer(const EntityContainer&) = delete;
		EntityContainer& operator=(EntityContainer&) = delete;
	};

private:
	EntityManager(const EntityManager&) = delete;
	EntityManager& operator=(EntityManager&) = delete;

	std::unordered_map<EntityTypeID, IEntityContainer*> m_registry;
	std::vector<EntityID> m_pendingDestroy;
	size_t m_numPendingDestroy;

	std::unordered_map<EntityID, IEntity*> m_handleTable;

	ComponentManager* m_componentManager;

	// return or create an entity container for entities of type T
	template<typename T>
	EntityContainer<T>* GetEntityContainer()
	{
		EntityTypeID typeID = T::STATIC_ENTITY_TYPE_ID;

		auto it = m_registry.find(typeID);
		EntityContainer<T>* container = nullptr;

		if(it == m_registry.end())
		{
			container = new EntityContainer<T>();
			m_registry[typeID] = container;
		}
		else
			container = (EntityContainer<T>*)it->second;

		assert(container != nullptr);
		return container;
	}


	uint64_t m_lastID;

public:
	EntityManager(ComponentManager* componentManager);
	~EntityManager();

	template<typename T, typename... Args>
	EntityID CreateEntity(Args... args)
	{
		void* pMemory = GetEntityContainer<T>()->CreateObject();

		((T*)pMemory)->m_id = m_lastID++;
		((T*)pMemory)->m_componentManager = m_componentManager;

		IEntity* entity = new(pMemory) T(std::forward<Args>(args)...);
	}

	void DestroyEntity(EntityID id);
	void RemoveDestroyedEntities();

	IEntity* GetEntity(EntityID id) { return m_handleTable[id]; }


};
