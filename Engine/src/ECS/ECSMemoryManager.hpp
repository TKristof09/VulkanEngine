#pragma once

#include "Memory/StackAllocator.hpp"

#define ECS_GLOBAL_MEMORY_SIZE 134217728 // 128 MB https://www.gbmb.org/mb-to-bytes

//TODO check memory leeks
class ECSMemoryManager
{
public:
	static ECSMemoryManager& GetInstance()
	{
		static ECSMemoryManager instance;
		return instance;
	}

	void* Allocate(size_t size)
	{
		return m_allocator->Allocate(size, alignof(uint8_t));
	}
	void Free(void* p)
	{
		m_allocator->Free(p);
	}
private:
	ECSMemoryManager()
	{
		m_globalMemory = malloc(ECS_GLOBAL_MEMORY_SIZE);
		m_allocator = new StackAllocator(ECS_GLOBAL_MEMORY_SIZE, m_globalMemory);
	}
	// these are to prevent copy of the singleton
	ECSMemoryManager(ECSMemoryManager const&) = delete;
	void operator=(ECSMemoryManager const&) = delete;

	StackAllocator* m_allocator;
	void* m_globalMemory;
};
