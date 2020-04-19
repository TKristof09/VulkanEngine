#pragma once

#include "ECS/Types.hpp"
#include "IComponent.hpp"
#include "Memory/MemoryChunkAllocator.hpp"

#include <unordered_map>

#define CHUNK_SIZE 512

class ComponentManager
{

	class IComponentContainer
	{
	public:
		virtual ~IComponentContainer() {}

		virtual void DestroyComponent(IComponent* component) = 0;
	};

	template<typename T>
	class ComponentContainer : public IComponentContainer, public MemoryChunkAllocator<T, CHUNK_SIZE>
	{
	public:
		ComponentContainer() {}
		virtual ~ComponentContainer() {}

		virtual void DestroyComponent(IComponent* component) override
		{
			component->~IComponent();
			this->DestroyObject(component);
		}
	};


	std::unordered_map<ComponentTypeID, IComponentContainer*> m_registry;
	std::unordered_map<EntityID, std::unordered_map<ComponentTypeID, ComponentID>> m_entityComponentMap;
	std::unordered_map<ComponentID, IComponent*> m_componentMap;
	template<typename T>
	ComponentContainer<T>* GetComponentContainer()
	{
		ComponentTypeID typeId = T::STATIC_COMPONENT_TYPE_ID;

		auto it = m_registry.find(typeId);
		ComponentContainer<T>* container = nullptr;

		if(it == m_registry.end())
		{
			container = new ComponentContainer<T>();
			m_registry[typeId] = container;
		}
		else
			container = (ComponentContainer<T>*)it->second;

		return container;
	}

	uint64_t m_lastID;


public:
	ComponentManager();
	~ComponentManager();

	template<typename T, typename ...Args>
	T* AddComponent(const EntityID entityId, Args... args)
	{
		ComponentTypeID typeId = T::STATIC_COMPONENT_TYPE_ID;

		auto it = m_entityComponentMap[entityId].find(typeId);
		assert(it != m_entityComponentMap[entityId].end() && "An entity can have only on of each component type");

		void* pMemory = GetComponentContainer<T>()->CreateObject();

		ComponentID id = m_lastID++;
		((T*)pMemory)->m_id = id;

		IComponent* component = new(pMemory) T(std::forward<Args>(args)...);

		component->m_parent = entityId;

		m_entityComponentMap[entityId][typeId] = id;

		return (T*)component;
	}
	template<class T>
	void RemoveComponent(const EntityID entityId)
	{
		ComponentTypeID typeId = T::STATIC_COMPONENT_TYPE_ID;
		ComponentID id = m_entityComponentMap[entityId][typeId];

		IComponent* component = m_componentMap[id];

		assert(component != nullptr);

		GetComponentContainer<T>()->DestroyComponent(component);

		m_entityComponentMap[entityId].erase(typeId);

		m_componentMap.erase(id);
	}
	void RemoveAllComponents(const EntityID entityId);

	template<class T>
	T* GetComponent(const EntityID entityId)
	{
		ComponentTypeID typeId = T::STATIC_COMPONENT_TYPE_ID;

		auto it = m_entityComponentMap[entityId].find(typeId);

		if(it == m_entityComponentMap[entityId].end())
			return nullptr;

		return (T*)m_componentMap[it->second];
	}


	template<typename T>
	typename ComponentContainer<T>::iterator begin()
	{
		return GetComponentContainer<T>()->begin();
	}

	template<typename T>
	typename ComponentContainer<T>::iterator end()
	{
		return GetComponentContainer<T>()->end();
	}

};
