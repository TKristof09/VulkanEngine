#pragma once

#include <list>
#include "PoolAllocator.hpp"
#include "ECS/ECSMemoryManager.hpp"

template<typename T, size_t MAX_OBJECTS>
class MemoryChunkAllocator
{
	static const size_t ALLOC_SIZE = GetAlignedSize(sizeof(T), alignof(T)) * MAX_OBJECTS;

public:
	using ObjectList = std::list<T*>;
	class MemoryChunk
	{
	public:
		PoolAllocator* allocator;
		ObjectList objects;

		uintptr_t chunkStart;
		uintptr_t chunkEnd;

		MemoryChunk(PoolAllocator* allocator):
			allocator(allocator)
		{
			chunkStart = (uintptr_t)allocator->GetMemoryStartAddress();
			chunkEnd = chunkStart + ALLOC_SIZE;
			objects.clear();
		}

	};

	using MemoryChunks = std::list<MemoryChunk*>;

	class iterator
	{
	public:
		using value_type = T*;
		using iterator_category = std::forward_iterator_tag;
		using pointer = T**;
		using reference = T*&;
		using difference_type = uint64_t;

	private:
		typename MemoryChunks::iterator m_currentChunk;
		typename MemoryChunks::iterator m_end;
		typename ObjectList::iterator m_currentObject;

		friend class MemoryChunkAllocator;

		iterator(typename MemoryChunks::iterator begin, typename MemoryChunks::iterator end):
			m_currentChunk(begin),
			m_end(end)
		{
			if(begin != end)
				m_currentObject = (*m_currentChunk)->objects.begin();
			//else
			//	m_currentObject = NULL;//(*std::prev(m_end))->objects.end();
		}

	public:
		inline iterator& operator++()
		{
			m_currentObject++;

			if(m_currentObject == (*m_currentChunk)->objects.end())
			{
				m_currentChunk++;
				if(m_currentChunk != m_end)
					m_currentObject = (*m_currentChunk)->objects.begin(); // set object iterator to start of next chunk
				else
					m_currentObject = {}; // so that we don't get "list iterators not compatible when comparing the iterator to end iterator(which leaves m_currentObject uninitialized)
			}
			return *this;
		}
        // TODO is this even correct? temp is a local variable
		inline iterator& operator++(int)
		{
			iterator temp = *this;
			++(*this);
			return temp;
		}

		inline T* operator*() const { return *m_currentObject; }
		inline T* operator->() const { return *m_currentObject; }

		inline bool operator==(const iterator& other) const { return (this->m_currentChunk == other.m_currentChunk) && (this->m_currentObject == other.m_currentObject); }
		inline bool operator!=(const iterator& other) const { return (this->m_currentChunk != other.m_currentChunk) || (this->m_currentObject != other.m_currentObject); }

	};


protected:
	MemoryChunks m_chunks;


public:
	MemoryChunkAllocator()
	{
		PoolAllocator* allocator = new PoolAllocator(ALLOC_SIZE, ECSMemoryManager::GetInstance().Allocate(ALLOC_SIZE), sizeof(T), alignof(T));
		//m_chunks.push_back(new MemoryChunk(allocator));
	}
	virtual ~MemoryChunkAllocator()
	{

		for (auto chunk : m_chunks)
		{
			for (auto obj : chunk->objects)
				((T*)obj)->~T();
			chunk->objects.clear();

			ECSMemoryManager::GetInstance().Free((void*)chunk->allocator->GetMemoryStartAddress());

			delete chunk->allocator;
			delete chunk;
		}
	}

	void* CreateObject()
	{
		void* slot = nullptr;

		// find next free slot
		for (auto chunk : m_chunks)
		{
			if(chunk->objects.size() > MAX_OBJECTS)
				continue;
			slot = chunk->allocator->Allocate(sizeof(T), alignof(T));
			if(slot != nullptr)
			{
				chunk->objects.push_back((T*)slot);
				break;
			}
		}
		// chunks are full -> allocate a new one
		if(slot == nullptr)
		{
			PoolAllocator* allocator = new PoolAllocator(ALLOC_SIZE, ECSMemoryManager::GetInstance().Allocate(ALLOC_SIZE), sizeof(T), alignof(T));
			MemoryChunk* chunk = new MemoryChunk(allocator);
			m_chunks.push_front(chunk); // push to front because this way the search to find free slot is faster
			slot = chunk->allocator->Allocate(sizeof(T), alignof(T));
			assert(slot != nullptr && "Failed to create new object, out of memory??");
			chunk->objects.clear();
			chunk->objects.push_back((T*)slot);
		}

		return slot;
	}

	void FreeObject(void* object)
	{
		uintptr_t p = (uintptr_t)object;

		for (auto chunk : m_chunks)
		{
			if(chunk->chunkStart <= p && p < chunk->chunkEnd)
			{
				// destructor should already have been called
				chunk->objects.remove((T*)object);
				chunk->allocator->Free(object);
				return;
			}
		}
	}


	inline iterator begin() { return iterator(m_chunks.begin(), m_chunks.end()); }
	inline iterator end() { return iterator(m_chunks.end(), m_chunks.end()); }


};
