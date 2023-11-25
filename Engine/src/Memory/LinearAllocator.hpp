#pragma once

#include "Allocator.hpp"

/*
        Allocates memory in a linear way.
              first          2   3   4
            allocatation     alloaction
                v            v   v   v
        |=================|=====|==|======| .... |
        ^                                        ^
        Initial                                  Last possible
        memory                                   memory address
        address                                  (memoryStart + size)
        (memoryStart)
        memory only can be freed by clearing all allocations
*/
class LinearAllocator : public Allocator
{
public:
    LinearAllocator(size_t size);
    ~LinearAllocator();
    void* Allocate(size_t size, uint8_t alignment) override;
    void Free(void* p) override;
    void Clear() override;
};
