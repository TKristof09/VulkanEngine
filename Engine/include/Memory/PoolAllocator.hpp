#pragma once

#include "Allocator.hpp"

class PoolAllocator : public Allocator
{
public:
	PoolAllocator(size_t size, const void* memoryStart, size_t objectSize, uint8_t alignment);
	virtual ~PoolAllocator();
	virtual void* Allocate(size_t size, uint8_t alignment) override;
	virtual void  Free(void* p) override;
	virtual void  Clear() override;

private:
	void** m_freeList;
	size_t m_objectSize;
	uint8_t m_alignment;
};

