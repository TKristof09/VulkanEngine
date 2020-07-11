#include "Memory/PoolAllocator.hpp"
#include <cmath>

PoolAllocator::PoolAllocator(size_t size, const void* memoryStart, size_t objectSize, uint8_t alignment):
	Allocator(size, memoryStart),
	m_objectSize(objectSize),
	m_alignment(alignment)
{
	Clear(); //	construct the free list
}
PoolAllocator::~PoolAllocator()
{
	m_freeList = nullptr;
}

void* PoolAllocator::Allocate(size_t size, uint8_t alignment)
{
	if(m_freeList == nullptr) // no free space available
		return nullptr;

	void* p = m_freeList; // get free slot

	m_freeList = (void**)(*m_freeList); // point to next free slot, *m_freeList is the first element of the list

	m_memoryUsed += m_objectSize;
	m_allocationsCount++;

	return p;
}

void PoolAllocator::Free(void* p)
{
	// insert it to the start of free list
	*((void**)p) = m_freeList; // make it point to the start of free list
	m_freeList = (void**)p;

	m_memoryUsed -= m_objectSize;
	m_allocationsCount--;
}

void PoolAllocator::Clear()
{
	uint8_t padding = GetPadding(m_memoryStartAddress, m_alignment);
	size_t numObjects = (size_t)floor((m_memorySize - padding) / m_objectSize);

	union
	{
		void* asVoidPtr;
		uintptr_t asUintPtr;
	};
	asVoidPtr = (void*)m_memoryStartAddress;
	// align the start address
	asUintPtr += padding;

	m_freeList = (void**)asVoidPtr;

	void** p = m_freeList;
	for (int i = 0; i < (numObjects - 1); ++i)
	{
		*p = (void*)((uintptr_t)p + m_objectSize); // set each element to point to the next free slot
		p = (void**)*p; // increment p
	}
	*p = nullptr; // last slot points to nullptr to indicate that it is the last
}

