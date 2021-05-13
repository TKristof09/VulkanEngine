#pragma once


#include "ECS/Types.hpp"
#include "ECS/IComponent.hpp"
#include "Memory/MemoryChunkAllocator.hpp"
#include "ECS/CoreEvents/ComponentEvents.hpp"
#include "Core/Events/EventHandler.hpp"
#include "Core/Scene/Scene.hpp"

#include <unordered_map>
#include <forward_list>
#include <queue>
#include <map>

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
		friend class ComponentManager;
	public:
		ComponentContainer() {}
		virtual ~ComponentContainer() {}

		virtual void DestroyComponent(IComponent* component) override
		{
			component->~IComponent();
			this->FreeObject(component);
		}
	};

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



	std::unordered_map<ComponentTypeID, IComponentContainer*> m_registry;
	std::unordered_map<EntityID, std::unordered_map<ComponentTypeID, ComponentID>> m_entityComponentMap;
	std::unordered_map<ComponentID, IComponent*> m_componentMap;
	std::queue<IComponent*> m_componentsToRemove;

	Scene m_scene;
	uint64_t m_lastID;

public:
	ComponentManager(Scene scene);
	~ComponentManager();

	void DestroyRemovedComponents();

	template<typename T, typename ...Args>
	T* AddComponent(const EntityID entityId, Args... args)
	{
		const ComponentTypeID typeId = T::STATIC_COMPONENT_TYPE_ID;

		auto it = m_entityComponentMap[entityId].find(typeId);
		

		if(it != m_entityComponentMap[entityId].end()) // an entity can only have one of each component type
			return (T*)m_componentMap[it->second];
		
		void* pMemory = GetComponentContainer<T>()->CreateObject();

		ComponentID id = m_lastID++;

		IComponent* component = new(pMemory) T(std::forward<Args>(args)...);

		component->m_id = id;
		component->m_owner = entityId;
		component->m_typeID = typeId;

		m_entityComponentMap[entityId][typeId] = id;
		m_componentMap[id] = component;

		ComponentAdded<T> e;
		e.entity = entityId;
		e.component = (T*)component;
		m_scene.eventHandler->Send<ComponentAdded<T>>(e);

		return (T*)component;
	}
	template<typename T>
	void RemoveComponent(const EntityID entityId)
	{
		const ComponentTypeID typeId = T::STATIC_COMPONENT_TYPE_ID;
		ComponentID id = m_entityComponentMap[entityId][typeId];

		IComponent* component = m_componentMap[id];

		assert(component != nullptr);

		m_componentsToRemove.push(component);

		m_entityComponentMap[entityId].erase(typeId);

		m_componentMap.erase(id);

		ComponentRemoved<T> e;
		e.entity = entityId;
		e.component = (T*)component;
		m_scene.eventHandler->Send<ComponentRemoved<T>>(e);

	}
	void RemoveAllComponents(const EntityID entityId);

	template<typename T>
	T* GetComponent(const EntityID entityId)
	{
		ComponentTypeID typeId = T::STATIC_COMPONENT_TYPE_ID;

		auto it = m_entityComponentMap[entityId].find(typeId);

		if(it == m_entityComponentMap[entityId].end())
		{
			LOG_ERROR("Entity with id: {0} doesn't have a {1} component", entityId, typeid(T).name());
			return nullptr;
		}

		return (T*)m_componentMap[it->second];
	}

	std::map<ComponentTypeID, IComponent*> GetAllComponents(EntityID entity) // we use an ordered map to guarantee that the components get the same order every time, i dont actually care about the order for now
	{
		std::map<ComponentTypeID, IComponent*> result;
		for(auto& [typeID, componentID] : m_entityComponentMap[entity])
		{
			result[typeID] = m_componentMap[componentID];
		}
		return result;
	}

	template<typename T>
	bool HasComponent(const EntityID entity)
	{
		return m_entityComponentMap[entity].find(T::STATIC_COMPONENT_TYPE_ID) != m_entityComponentMap[entity].end();
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

	template<typename T>
	ComponentContainer<T>& GetComponents()
	{
		return *GetComponentContainer<T>();
	}

	// TODO Improve this (really not happy with the implementation)
	// WARNING: This copies(or moves) the components around, also O(n log n)
	template<typename T, typename Compare>
	void Sort(Compare comp)
	{
		auto container = GetComponentContainer<T>();
		std::forward_list<T*> objList(container->begin(), container->end());
		objList.sort(comp);
		std::list<T> copyList;
		for (auto* obj : objList)
		{
			copyList.push_back(std::move(*obj));
		}
		auto itList = copyList.begin();
		for (auto* chunk : container->m_chunks)
		{
			for (auto* obj : chunk->objects)
			{
				*obj = std::move(*itList);
				m_componentMap[obj->m_id] = (IComponent*)(obj);
				itList++;
			}
		}
	}

};
