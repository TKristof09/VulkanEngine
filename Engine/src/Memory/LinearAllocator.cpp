#include "Memory/LinearAllocator.hpp"

LinearAllocator::LinearAllocator(size_t size)
    : Allocator(size, new uint8_t[size]) {}

LinearAllocator::~LinearAllocator()
{
    Clear();
    delete[](uint8_t*)m_memoryStartAddress;
}

void* LinearAllocator::Allocate(size_t size, uint8_t alignment)
{
    union
    {
        void* asVoidPtr;
        uintptr_t asUintPtr;
    };

    asVoidPtr  = (void*)m_memoryStartAddress;
    asUintPtr += m_memoryUsed;  // determine current address

    uint8_t padding = GetPadding(asVoidPtr, alignment);

    if(m_memoryUsed + size + padding > m_memorySize)  // check if there is enough space
        return nullptr;

    asUintPtr += padding;  // this is the start of the allocated memory with correct alignment

    m_memoryUsed += size + padding;  // reserve the memory for this allocation
    m_allocationsCount++;

    return asVoidPtr;  // return the start address of the allocated memory
}

void LinearAllocator::Free(void*  /*p*/)
{
    assert(false && "Linear allocator doesn't support free. Use clear instead");
}

void LinearAllocator::Clear()
{
    // just reset the whole memory
    m_memoryUsed       = 0;
    m_allocationsCount = 0;
}
