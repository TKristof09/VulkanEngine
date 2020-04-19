#pragma once

#include "Memory/LinearAllocator.hpp"
#include "ECS/Types.hpp"
#include "ECS/ISystem.hpp"

#include <unordered_map>

#define ECS_SYSTEM_MEMORY_SIZE 8388608 // 8MB
//TODO system dependecies
class SystemManager
{
private:
	LinearAllocator* m_allocator;
	std::unordered_map<SystemTypeID, ISystem*> m_registry;
	SystemManager(const SystemManager&) = delete;
	SystemManager& operator=(SystemManager&) = delete;
public:
	SystemManager();
	~SystemManager();

	void Update(float dt);

	template<typename T, typename... Args>
	T* AddSystem(Args... args)
	{
		SystemTypeID id = T::STATIC_SYSTEM_TYPE_ID;

		auto it = m_registry.find(id);
		if(it != m_registry.end())
			return (T*)it->second;

		T* system = nullptr;

		void* pMemory = m_allocator->Allocate(sizeof(T), alignof(T));
		((T*)pMemory)->m_systemManager = this;

		system = new(pMemory) T(std::forward<Args>(args)...);
		m_registry[id] = system;
		return system;
	}

	template<typename T>
	T* GetSystem() const
	{
		auto it = m_registry.find(T::STATIC_SYSTEM_TYPE_ID);
		return it != m_registry.end() ? (T*)it->second : nullptr;
	}

	template<typename T>
	void EnableSystem()
	{
		SystemTypeID id = T::STATIC_SYSTEM_TYPE_ID;
		auto it = m_registry.find(id);
		if(it != m_registry.end())
		{
			if(it->second->m_enabled == true)
				return;
			it->second->m_enabled = true;
		}
	}

	template<typename T>
	void DisableSystem()
	{
		SystemTypeID id = T::STATIC_SYSTEM_TYPE_ID;
		auto it = m_registry.find(id);
		if(it != m_registry.end())
		{
			if(it->second->m_enabled == false)
				return;
			it->second->m_enabled = false;
		}
	}

};
