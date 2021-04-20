#include "Utils/DebugUIElements.hpp"
#include "Utils/DebugUI.hpp"

DebugUIWindow::~DebugUIWindow()
{
    m_debugUI->RemoveWindow(m_index);
}
