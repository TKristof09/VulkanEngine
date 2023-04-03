#include "Utils/DebugUIElements.hpp"
#include "Utils/DebugUI.hpp"
#include <vulkan/vulkan.h>

VkSampler UIImage::s_sampler = VK_NULL_HANDLE;

DebugUIWindow::~DebugUIWindow()
{
    m_debugUI->RemoveWindow(m_index);
}
