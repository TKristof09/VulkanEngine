#pragma once

#include "Allocator.hpp"

class StackAllocator : public Allocator
{
public:
	StackAllocator(size_t size, const void* memoryStart);
	~StackAllocator();

	virtual void* Allocate(size_t size, uint8_t alignment) override;
	virtual void  Free(void* p) override;
	virtual void Clear() override;

private:
	struct Header
	{
		uint8_t padding;
	};
};
