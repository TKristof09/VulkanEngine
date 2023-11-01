#include "Utils/DebugUIElements.hpp"
#include "Utils/DebugUI.hpp"
#include <vulkan/vulkan.h>

DebugUIWindow::~DebugUIWindow()
{
    m_debugUI->RemoveWindow(m_index);
}
