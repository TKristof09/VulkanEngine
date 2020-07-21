#include "Memory/StackAllocator.hpp"

StackAllocator::StackAllocator(size_t size, const void* memoryStart):
	Allocator(size, memoryStart) {}

StackAllocator::~StackAllocator()
{
	Clear();
}

void* StackAllocator::Allocate(size_t size, uint8_t alignment)
{
	union
	{
		void* asVoidPtr;
		uintptr_t asUintPtr;
		Header* asHeader;
	};

	asVoidPtr = (void*)m_memoryStartAddress;

	asUintPtr += m_memoryUsed; // this is the current address

	uint8_t padding = GetPadding(asVoidPtr, alignment, sizeof(Header));

	if(m_memoryUsed + size + padding > m_memorySize) // check if there is enough space left
		return nullptr; // not enough space

	asHeader->padding = padding; // store alignment in header

	asUintPtr += padding; // determine aligned memory address

	m_memoryUsed += size + padding;
	m_allocationsCount++;

	return asVoidPtr;
}

void StackAllocator::Free(void* p)
{
	union
	{
		void* asVoidPtr;
		uintptr_t asUintPtr;
		Header* asHeader;
	};

	asVoidPtr = p;

	asUintPtr -= sizeof(Header); // get header info

	// free the memory
	m_memoryUsed -= (uintptr_t)m_memoryStartAddress + m_memoryUsed - (uintptr_t)p - asHeader->padding;
	m_allocationsCount--;
}

void StackAllocator::Clear()
{
	m_memoryUsed = 0;
	m_allocationsCount = 0;
}
