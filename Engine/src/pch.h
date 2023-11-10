#pragma once

#include <cstdint>
#include <cassert>
#include <iostream>
#include <array>
#include <vector>
#include "Utils/Log.hpp"
#include "Utils/Profiling.hpp"
#include "vulkan/vk_enum_string_helper.h"

#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#ifdef VDEBUG
inline void VK_CHECK(int result, const std::string& error_message)
{
    if(result != 0 && result != -1000011001)  // 0 is VK_SUCCESS, -1000011001 is VK_ERROR_VALIDATION_FAILED which gets thrown even on warnings
    {
        std::string error;
        error += string_VkResult((VkResult)result);
        error += " | ";
        error += error_message;
        LOG_ERROR(error);
        throw std::runtime_error(error);
    }
}

#else
inline void VK_CHECK(int result, const std::string& error_message)
{
    return;
}
#endif
