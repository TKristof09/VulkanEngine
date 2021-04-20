#pragma once
#include "imgui/imgui.h"
#include <unordered_map>
#include <glm/glm.hpp>
#include <map>

class DebugUI;

class DebugUIElement
{
public:
    virtual ~DebugUIElement() = default;
    virtual void Update() = 0;
    virtual void Deselect(bool recursive = false) {};

    std::string GetName() const
    {
        return m_name;
    }
    void SetName(const std::string& name)
    {
        m_name = name;
    }
    void* GetUserPtr() const
    {
        return m_userPtr;
    }
    template<typename T>
    void SetUserPtr(const T& data)
    {
        m_userPtr = (void*)data;
    }
protected:
    std::string m_name;
    const void* m_id = nullptr;
    void* m_userPtr = nullptr;
};

class Text : public DebugUIElement
{
public:
    Text(const std::string& text, const void* id = nullptr) :
        m_text(text)
    {
        m_name = text;
        m_id = id;
    }
    void Update() override
    {
        if(m_id)
            ImGui::PushID(m_id);

        ImGui::Text(m_text.c_str());

        if(m_id)
            ImGui::PopID();
    }
private:
    std::string m_text;
};

class DragFloat : public DebugUIElement
{
    DragFloat(float* value, const std::string& name = "Float", float min = 0, float max = 0, const void* id = nullptr) :
        m_min(min),
        m_max(max),
        m_value(value)
    {
        m_name = name;
        m_id = id;
    };

    void Update() override
    {
        if (m_id)
            ImGui::PushID(m_id);

        ImGui::DragFloat(m_name.c_str(), m_value, 1, m_min, m_max);
        if(m_id)
            ImGui::PopID();
    }

    float* GetValue() const
    {
        return m_value;
    }
private:
    float m_min, m_max;
    float* m_value;
};

class DragVector3 : public DebugUIElement
{
public:
    DragVector3(glm::vec3* value, const std::string& name = "Vector3", float min = 0, float max = 0, const void* id = nullptr) :
        m_min(min),
        m_max(max),
        m_value(value)
    {
        m_name = name;
        m_id = id;
    };
    
    void Update() override
    {
        if (m_id)
            ImGui::PushID(m_id);

        ImGui::DragFloat3(m_name.c_str(), reinterpret_cast<float*>(m_value), 1, m_min, m_max);

        if (m_id)
            ImGui::PopID();
    }
    
    glm::vec3* GetValue() const
    {
        return m_value;
    }
private:
    float m_min, m_max;
    glm::vec3* m_value;
};

class DragVector4 : public DebugUIElement
{
public:
    DragVector4(glm::vec4* value, const std::string& name = "Vector4", float min = 0, float max = 0, const void* id = nullptr) :
        m_min(min),
        m_max(max),
        m_value(value)
    {
        m_name = name;
        m_id = id;
    };

    void Update() override
    {
        if (m_id)
            ImGui::PushID(m_id);

        ImGui::DragFloat4(m_name.c_str(), reinterpret_cast<float*>(m_value), 1, m_min, m_max);

        if (m_id)
            ImGui::PopID();
    }

    glm::vec4* GetValue() const
    {
        return m_value;
    }
private:
    float m_min, m_max;
    glm::vec4* m_value;
};

class DragQuaternion : public DebugUIElement
{
public:
    DragQuaternion(glm::quat* value, const std::string& name = "Quaternion", float min = 0, float max = 0, const void* id = nullptr) :
        m_min(min),
        m_max(max),
        m_value(value)
    {
        m_name = name;
        m_id = id;
    };

    void Update() override
    {
        if (m_id)
            ImGui::PushID(m_id);

        ImGui::DragFloat4(m_name.c_str(), reinterpret_cast<float*>(m_value), 1, m_min, m_max);

        if (m_id)
            ImGui::PopID();
    }

    glm::quat* GetValue() const
    {
        return m_value;
    }
private:
    float m_min, m_max;
    glm::quat* m_value;
};
#if 0
class ColorEdit3 : public DebugUIElement
{
public:
    ColorEdit3(Color* value, const std::string& name = "Color", const void* id = nullptr):
        m_value(value)
    {
        m_name = name;
        m_id = id;
    }
    void Update() override
    {
        if (m_id)
            ImGui::PushID(m_id);

        ImGui::ColorEdit3(m_name.c_str(), &m_value->r);

        if (m_id)
            ImGui::PopID();
    }
    Color* GetValue() const
    {
        return m_value;
    }
private:
    Color* m_value;
};

class ColorEdit4 : public DebugUIElement
{
public:
    ColorEdit4(Color* value, const std::string& name = "Color", const void* id = nullptr):
        m_value(value)

    {
        m_name = name;
        m_id = id;
    }
    void Update() override
    {
        if (m_id)
            ImGui::PushID(m_id);

        ImGui::ColorEdit4(m_name.c_str(), &m_value->r);

        if (m_id)
            ImGui::PopID();
    }

    Color* GetValue() const
    {
        return m_value;
    }
private:
    Color* m_value;
};
#endif
class CheckBox : public DebugUIElement
{
public:
    CheckBox(bool* value, const std::string& name = "CheckBox", const void* id = nullptr):
        m_value(value)
    {
        m_name = name;
        m_id = id;
    }
    void Update() override
    {
        if (m_id)
            ImGui::PushID(m_id);

        ImGui::Checkbox(m_name.c_str(), m_value);

        if (m_id)
            ImGui::PopID();
    }

    bool GetValue() const
    {
        return *m_value;
    }

private:
    bool* m_value;
};

class Button : public DebugUIElement
{
public: 
    Button(const std::string& name = "Button", bool* value = nullptr, const void* id = nullptr):
        m_value(value),
        m_callback(nullptr)
    {
        m_name = name;
        m_id = id;
    }

    void Update() override
    {
        if (m_id)
            ImGui::PushID(m_id);
        bool val = false;
        if(m_value)
            *m_value = ImGui::Button(m_name.c_str());
        else
            val = ImGui::Button(m_name.c_str());

        if ((m_value && *m_value) || val)
        {
            if(m_callback)
                m_callback(this);
        }

        if (m_id)
            ImGui::PopID();
    }

    void RegisterCallback(std::function<void(Button*)> callback)
    {
        m_callback = callback;
    }

    bool GetValue() const
    {
        return *m_value;
    }

private:
    bool* m_value;
    std::function<void(Button*)> m_callback;
    

};

class TreeNode : public DebugUIElement
{
public:
    TreeNode(const std::string& name, bool* value = nullptr, const void* id = nullptr) :
        m_value(value),
        m_callback(nullptr)
    {
        m_name = name;
        m_id = id;
    }


    void Update() override
    {

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        if(m_isSelected)
            flags |= ImGuiTreeNodeFlags_Selected;

        if(m_elements.empty())
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        if (m_id)
            ImGui::PushID(m_id);

        ImGui::AlignTextToFramePadding();
        
        bool isOpened = ImGui::TreeNodeEx(m_name.c_str(), flags) && (!m_elements.empty());
        
        if(m_value)
            *m_value = isOpened;

        if(ImGui::IsItemClicked())
        {
            if(m_callback)
                m_callback(this);
        }

        if(isOpened)
        {
            for (DebugUIElement* element : m_elements)
            {
                element->Update();
            }
            ImGui::TreePop();
        }

        if (m_id)
            ImGui::PopID();
    }

    virtual void Deselect(bool recursive = false) override
    {
        m_isSelected = false;

        if(recursive)
        {
            for(auto& element : m_elements)
            {
                element->Deselect(true);
            }
        }

    }

    void AddElement(DebugUIElement* element)
    {
        m_elements.push_back(element);
    }

    std::vector<DebugUIElement*> GetElements() const
    {
        return m_elements;
    }

    bool GetValue() const
    {
        return *m_value;
    }

    void RegisterCallback(std::function<void(TreeNode*)> callback)
    {
        m_callback = callback;
    }

    void SetIsSelected(bool state)
    {
        m_isSelected = state;
    }

private:
    bool* m_value;
    std::vector<DebugUIElement*> m_elements;
    std::function<void(TreeNode*)> m_callback;
    
    bool m_isSelected = false;

};

class Separator : public DebugUIElement
{
public:
    Separator(const std::string& name = "Separator")
    {
        m_name = name;
    }
    void Update() override
    {
        ImGui::Separator();
    }
};

class DebugUIWindow
{
public:
    DebugUIWindow(const std::string& name = "Debug", bool opened = true) : m_debugUI(nullptr),                                                    m_index(0),       
      m_name(name), 
      m_opened(opened) {}

    ~DebugUIWindow();

    void AddElement(DebugUIElement* element, uint32_t column = 1)
    {
        while (m_columnHeights.size() < column)
        {
            m_columnHeights.push_back(0);
        }
        auto it = m_registry.find(element->GetName());
        if (it != m_registry.end())
        {
            LOG_ERROR("DebugUIWindow: {0} already contains an element named: {1}", m_name, element->GetName());
            return;
        }
        
        std::pair<uint32_t, uint32_t> coords = { m_columnHeights[column - 1]++, column };
        m_elements[coords] = element;
        m_registry[element->GetName()] = coords;
    }

    DebugUIElement* GetElement(const std::string& name, uint32_t column = 1)
    {
        if (m_columnHeights.size() < column)
        {
            LOG_ERROR("GetElement called on debugUIWindow: {0} with param column: {1}, but the window has {2} columns in total", m_name, column, m_columnHeights.size());
            return nullptr;
        }
        auto it = m_registry.find(name);
        if (it != m_registry.end())
        {
            return m_elements[m_registry[name]];
        }
        LOG_ERROR("Element: {0} couldn't be found in debugUIWindow: {1}", name, m_name);
        return nullptr;
    }

    void RemoveElement(const std::string& name, uint32_t column = 1)
    {
        if(m_columnHeights.size() < column)
        {
            LOG_ERROR("RemoveElement called on debugUIWindow: {0} with param column: {1}, but the window has {2} columns in total", m_name, column, m_columnHeights.size());
        }
        auto it = m_registry.find(name);
        if (it != m_registry.end())
        {
            m_elements.erase(it->second);
            m_registry.erase(it);
        }
    }

    void Clear()
    {
        m_registry.clear();
        m_elements.clear();
        m_columnHeights.clear();
    }

    void DeselectAll()
    {
        for(auto& [coords, element] : m_elements)
        {
            element->Deselect(true);
        }
    }

    void Update()
    {
        m_opened = ImGui::Begin(m_name.c_str(), &m_opened);
        if(m_opened)
        {
            if(!m_elements.empty())
            {
                ImGui::BeginTable("table1", m_columnHeights.size());

                uint32_t lastRow = 0;
                ImGui::TableNextRow();
                for(auto& [coords, element] : m_elements)
                {
                    auto [x, y] = coords;
                    if(x != lastRow)
                    {
                        lastRow = x;
                        ImGui::TableNextRow();
                    }

                    ImGui::TableSetColumnIndex(y - 1);

                    element->Update();
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }


private:
    friend class DebugUI;
    DebugUI* m_debugUI;
    uint32_t m_index;
    std::string m_name;
    bool m_opened;
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> m_registry;
    std::map<std::pair<uint32_t, uint32_t>, DebugUIElement*> m_elements;
    std::vector<uint32_t> m_columnHeights;
};
