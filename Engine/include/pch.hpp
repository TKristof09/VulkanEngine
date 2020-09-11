#pragma once

#include <stdint.h>
#include <cassert>
#include <iostream>
#include "Utils/Log.hpp"

#ifdef VDEBUG
// using int instead of VkResult enum beacuse i don't want to include vulkan here so this is just a dirty fix
inline void VK_CHECK(int result, const char* error_message)
{
    if (result != 0 && result != -1000011001) // 0 is VK_SUCCESS, -1000011001 is VK_ERROR_VALIDATION_FAILED which gets thrown even on warnings
        throw std::runtime_error(error_message);
}

#else
inline void VK_CHECK(int result, const char* error_message)
{
	return;
}
#endif

//eastl needs this
//void* operator new[](size_t size, const char* name, int flags, unsigned debugFlags, const char* file, int line);
//void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line);

