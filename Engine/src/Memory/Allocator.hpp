#pragma once

class Allocator
{
public:
	Allocator(const size_t memorySize, const void* memoryStartAddress):
		m_memorySize(memorySize),
		m_memoryStartAddress(memoryStartAddress),
		m_memoryUsed(0),
		m_allocationsCount(0)
	{
		std::memset((void*)memoryStartAddress, 0, memorySize);
	}
	virtual	~Allocator() {}

	virtual void* Allocate(size_t size, uint8_t alignment) = 0;
	virtual void  Free(void* p) = 0;
	virtual void  Clear() = 0;

	inline size_t GetMemorySize() const { return m_memorySize; }
	inline const void* GetMemoryStartAddress() const { return m_memoryStartAddress; }
	inline size_t GetMemoryUsed() const { return m_memoryUsed; }
	inline uint64_t GetAllocationCount() const { return m_allocationsCount; }
protected:
	const size_t m_memorySize;
	const void* m_memoryStartAddress;
	size_t m_memoryUsed;
	uint64_t m_allocationsCount;
};

static inline uint8_t GetPadding(const void* address, uint8_t alignment)
{
	uint8_t padding = alignment - ((uintptr_t)address & (uintptr_t)(alignment - 1));

	return padding == alignment ? 0 : padding;
}

static inline uint8_t GetPadding(const void* address, uint8_t alignment, uint8_t headerSize)
{
	uint8_t padding = GetPadding(address, alignment);

	uint8_t neededSpace = headerSize;

	if(padding < neededSpace)
	{
		neededSpace -= padding;

		padding += neededSpace;

		if(neededSpace % alignment != 0) // if it is no longer correctly aligned
			padding += alignment;
	}

	return padding;
}

static inline void* AlignForward(void* address, uint8_t alignment)
{
	return (void*)((uintptr_t)(address) + GetPadding(address, alignment));
}
