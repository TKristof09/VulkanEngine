#pragma once

#include <stdint.h>
#include <cassert>
#include <iostream>
#include <EASTL/string.h>
#include <EASTL/memory.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/vector.h>

#ifdef VDEBUG
// using int instead of VkResult enum beacuse i don't want to include vulkan here so this is just a dirty fix
inline void VK_CHECK(int result, const eastl::string& error_message)
{
    if (result != 0) // VK_SUCCESS is 0
        throw std::runtime_error(error_message.c_str());
}

#else
inline void VK_CHECK(int result, const eastl::string& error_message)
{
	return;
}
#endif

//eastl needs this
void* operator new[](size_t size, const char* name, int flags, unsigned debugFlags, const char* file, int line);
void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line);

