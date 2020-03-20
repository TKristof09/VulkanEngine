#pragma once

#include <stdint.h>
#include <memory>
#include <string>
#include <iostream>
#include <vector>

// using int instead of VkResult enum beacuse i don't want to include vulkan here so this is just a dirty fix
inline void VK_CHECK(int result, const std::string& error_message)
{
    if (result != 0) // VK_SUCCESS is 0
        throw std::runtime_error(error_message);
}

//#include <Windows.h> // fckin vulkan needed otherwise vulkan.hpp throws error (its a bug in their code)
