#pragma once
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <map>

#include "ECS/CoreComponents/Transform.hpp"
#include "Utils/DebugUIElements.hpp"
#include "Utils/FileDialog/FileDialog.hpp"
#include "Utils/Color.hpp"
#include "Rendering/Image.hpp"
#include "backends/imgui_impl_vulkan.h"
#include "Rendering/VulkanContext.hpp"

class DebugUI;

class DebugUIElement
{
public:
    virtual ~DebugUIElement() = default;
    virtual void Update()     = 0;
    virtual void Deselect(bool /*recursive = false*/) {}

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
    void* m_userPtr = nullptr;
    bool m_hasTag;
};

class Text : public DebugUIElement
{
public:
    Text(std::string& text) : m_text(text)
    {
        m_name = text;
    }
    Text(const std::string& text) : m_text(m_name)
    {
        m_name = text;
    }
    void SetText(const std::string& text)
    {
        m_text = text;
    }
    void Update() override
    {
        ImGui::PushID(this);

        ImGui::Text(m_text.c_str());

        ImGui::PopID();
    }

private:
    std::string& m_text;
};

class TextEdit : public DebugUIElement
{
public:
    TextEdit(std::string* text) : m_text(text)
    {
    }

    void Update() override
    {
        ImGui::PushID(this);
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;

        bool finished = ImGui::InputText("", m_text, flags);
        if(finished && m_callback)
            m_callback(this);

        ImGui::PopID();
    }

    void RegisterCallback(std::function<void(TextEdit*)> callback)
    {
        m_callback = callback;
    }

private:
    std::string* m_text;
    std::function<void(TextEdit*)> m_callback;
};

class DragFloat : public DebugUIElement
{
public:
    DragFloat(float* value, const std::string& name = "", float min = 0, float max = 0)
        : m_min(min),
          m_max(max),
          m_value(value)
    {
        m_hasTag = name != "";
        m_name   = name != "" ? name : std::to_string((uint64_t)this);
    };

    void Update() override
    {
        ImGui::PushID(this);

        if(m_hasTag)
        {
            ImGui::Text(m_name.c_str());
            ImGui::SameLine();
        }

        ImGui::DragFloat(m_name.c_str(), m_value, 1, m_min, m_max);

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

class DragVector2 : public DebugUIElement
{
public:
    DragVector2(glm::vec2* value, const std::string& name = "", float min = 0, float max = 0)
        : m_min(min),
          m_max(max),
          m_value(value)
    {
        m_hasTag = name != "";
        m_name   = name != "" ? name : std::to_string((uint64_t)this);
    };

    void Update() override
    {
        ImGui::PushID(this);

        if(m_hasTag)
        {
            ImGui::Text(m_name.c_str());
            ImGui::SameLine();
        }

        ImGui::DragFloat2("##", reinterpret_cast<float*>(m_value), 1, m_min, m_max);

        ImGui::PopID();
    }

    glm::vec2* GetValue() const
    {
        return m_value;
    }

private:
    float m_min, m_max;
    glm::vec2* m_value;
};

class DragVector3 : public DebugUIElement
{
public:
    DragVector3(glm::vec3* value, const std::string& name = "", float min = 0, float max = 0)
        : m_min(min),
          m_max(max),
          m_value(value)
    {
        m_hasTag = name != "";
        m_name   = name != "" ? name : std::to_string((uint64_t)this);
    };

    void Update() override
    {
        ImGui::PushID(this);

        if(m_hasTag)
        {
            ImGui::Text(m_name.c_str());
            ImGui::SameLine();
        }

        ImGui::DragFloat3("##", reinterpret_cast<float*>(m_value), 1, m_min, m_max);

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
    DragVector4(glm::vec4* value, const std::string& name = "", float min = 0, float max = 0)
        : m_min(min),
          m_max(max),
          m_value(value)
    {
        m_hasTag = name != "";
        m_name   = name != "" ? name : std::to_string((uint64_t)this);
    };

    void Update() override
    {
        ImGui::PushID(this);

        if(m_hasTag)
        {
            ImGui::Text(m_name.c_str());
            ImGui::SameLine();
        }

        ImGui::DragFloat4("##", reinterpret_cast<float*>(m_value), 1, m_min, m_max);

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
    DragQuaternion(glm::quat* value, const std::string& name = "", float min = 0, float max = 0)
        : m_min(min),
          m_max(max),
          m_value(value)
    {
        m_hasTag = name != "";
        m_name   = name != "" ? name : std::to_string((uint64_t)this);
    };

    void Update() override
    {
        ImGui::PushID(this);

        if(m_hasTag)
        {
            ImGui::Text(m_name.c_str());
            ImGui::SameLine();
        }

        ImGui::DragFloat4("##", reinterpret_cast<float*>(m_value), 1, m_min, m_max);

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

class DragEulerAngles : public DebugUIElement
{
public:
    DragEulerAngles(glm::quat* value, const std::string& name = "")
        : m_value(value),
          m_euler(glm::degrees(glm::eulerAngles(*value))),
          m_cachedValue(*value)
    {
        m_hasTag = name != "";
        m_name   = name != "" ? name : std::to_string((uint64_t)this);
    }

    void Update() override
    {
        if(*m_value != m_cachedValue)
        {
            m_euler       = glm::degrees(glm::eulerAngles(*m_value));
            m_cachedValue = *m_value;
        }
        ImGui::PushID(this);

        if(m_hasTag)
        {
            ImGui::Text(m_name.c_str());
            ImGui::SameLine();
        }
        bool valueChanged = ImGui::DragFloat3("##", reinterpret_cast<float*>(&m_euler), 1, -360, 360);

        ImGui::PopID();

        if(valueChanged)
        {
            *m_value      = glm::quat(glm::radians(m_euler));
            m_cachedValue = *m_value;
        }
    }


private:
    glm::quat* m_value;
    glm::vec3 m_euler;
    glm::quat m_cachedValue;
};

class ColorEdit3 : public DebugUIElement
{
public:
    ColorEdit3(Color* value, const std::string& name = "")
        : m_value(value)
    {
        m_hasTag = name != "";
        m_name   = name != "" ? name : std::to_string((uint64_t)this);
    }
    void Update() override
    {
        ImGui::PushID(this);

        if(m_hasTag)
        {
            ImGui::Text(m_name.c_str());
            ImGui::SameLine();
        }

        ImGui::ColorEdit3(m_name.c_str(), &m_value->r);

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
    ColorEdit4(Color* value, const std::string& name = "")
        : m_value(value)
    {
        m_hasTag = name != "";
        m_name   = name != "" ? name : std::to_string((uint64_t)this);
    }
    void Update() override
    {
        ImGui::PushID(this);

        if(m_hasTag)
        {
            ImGui::Text(m_name.c_str());
            ImGui::SameLine();
        }

        ImGui::ColorEdit4(m_name.c_str(), &m_value->r);

        ImGui::PopID();
    }

    Color* GetValue() const
    {
        return m_value;
    }

private:
    Color* m_value;
};

class CheckBox : public DebugUIElement
{
public:
    CheckBox(bool* value, const std::string& name = "")
        : m_value(value)
    {
        m_hasTag = name != "";
        m_name   = name != "" ? name : std::to_string((uint64_t)this);
    }
    void Update() override
    {
        ImGui::PushID(this);

        if(m_hasTag)
        {
            ImGui::Text(m_name.c_str());
            ImGui::SameLine();
        }

        ImGui::Checkbox(m_name.c_str(), m_value);


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
    Button(const std::string& name = "", bool* value = nullptr)
        : m_value(value),
          m_callback(nullptr)
    {
        m_hasTag = name != "";
        m_name   = name != "" ? name : std::to_string((uint64_t)this);
    }

    void Update() override
    {
        ImGui::PushID(this);
        bool val = false;
        if(m_value)
            *m_value = ImGui::Button(m_name.c_str());
        else
            val = ImGui::Button(m_name.c_str());

        if((m_value && *m_value) || val)
        {
            if(m_callback)
                m_callback(this);
        }


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

class FileSelector : public DebugUIElement
{
public:
    FileSelector(std::string* path)
        : m_path(path),
          m_callback(nullptr)
    {
        m_name = std::to_string((uint64_t)this);
    }
    void Update() override
    {
        ImGui::PushID(this);

        std::string fileName = m_path->substr(m_path->find_last_of("/") + 1);
        ImGui::Text(fileName.c_str());

        ImGui::SameLine();

        if(ImGui::Button("..."))
        {
            *m_path = FileDialog::OpenFile(nullptr);
            if(m_callback && *m_path != "")
                m_callback(this);
        }


        ImGui::PopID();
    }

    void RegisterCallback(std::function<void(FileSelector*)> callback)
    {
        m_callback = callback;
    }
    std::string GetPath() const { return *m_path; }

private:
    std::string* m_path;
    std::function<void(FileSelector*)> m_callback;
};

class TreeNode : public DebugUIElement
{
public:
    TreeNode(std::string& text, bool* value = nullptr)
        : m_value(value),
          m_callback(nullptr),
          m_text(text)
    {
        m_name = m_text;
    }
    TreeNode(const std::string& text, bool* value = nullptr)
        : m_value(value),
          m_callback(nullptr),
          m_text(m_name)
    {
        m_name = text;
    }


    void Update() override
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        if(m_isSelected)
            flags |= ImGuiTreeNodeFlags_Selected;

        if(m_elements.empty())
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;


        ImGui::PushID(this);

        ImGui::AlignTextToFramePadding();

        bool isOpened = ImGui::TreeNodeEx(m_text.c_str(), flags) && (!m_elements.empty());

        if(m_value)
            *m_value = isOpened;

        if(ImGui::IsItemClicked())
        {
            if(m_callback)
                m_callback(this);
        }

        if(isOpened)
        {
            for(auto element : m_elements)
            {
                element->Update();
            }
            ImGui::TreePop();
        }


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

    void AddElement(std::shared_ptr<DebugUIElement> element)
    {
        m_elements.push_back(element);
    }

    std::vector<std::shared_ptr<DebugUIElement>> GetElements() const
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
    std::string& m_text;
    std::vector<std::shared_ptr<DebugUIElement>> m_elements;
    std::function<void(TreeNode*)> m_callback;

    bool m_isSelected = false;
};

class Separator : public DebugUIElement
{
public:
    Separator(const std::string& name = "")
    {
        m_name = name != "" ? name : std::to_string((uint64_t)this);
    }
    void Update() override
    {
        ImGui::Separator();
    }
};

class Table : public DebugUIElement
{
public:
    Table(const std::string& name = "")
        : m_maxColumn(0)
    {
        m_name = name != "" ? name : std::to_string((uint64_t)this);
    }
    void Update() override
    {
        ImGui::PushID(this);
        if(!m_elements.empty())
        {
            ImGui::BeginTable("table1", m_maxColumn);

            uint32_t lastRow = 0;
            ImGui::TableNextRow();
            for(auto& [coords, element] : m_elements)
            {
                auto [x, y] = coords;
                while(x != lastRow)
                {
                    lastRow++;
                    ImGui::TableNextRow();
                }

                ImGui::TableSetColumnIndex(y - 1);

                element->Update();
            }
            ImGui::EndTable();
        }
        ImGui::PopID();
    }

    void AddElement(std::shared_ptr<DebugUIElement> element, uint32_t row, uint32_t column)
    {
        auto it = m_elements.find({row, column});
        if(it != m_elements.end())
            LOG_WARN("Table {0} already containts an element at {1}, {2}. It will be overwritten", m_name, row, column);

        m_elements[{row, column}] = element;

        m_maxColumn = column > m_maxColumn ? column : m_maxColumn;
    }

private:
    std::map<std::pair<uint32_t, uint32_t>, std::shared_ptr<DebugUIElement>> m_elements;
    uint32_t m_maxColumn;
};

class UIImage : public DebugUIElement
{
public:
    UIImage(Image* image)
        : m_image(image)
    {
        m_desc = ImGui_ImplVulkan_AddTexture(VulkanContext::GetTextureSampler(), m_image->GetImageView(), VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
    }
    void Update() override
    {
        ImGui::PushID(this);
        float width  = static_cast<float>(m_image->GetWidth());
        float height = static_cast<float>(m_image->GetHeight());
        float aspect = width / height;
        if(width > ImGui::GetContentRegionAvail().x)
        {
            width  = ImGui::GetContentRegionAvail().x;
            height = width / aspect;
        }

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Image(m_desc, ImVec2(width, height));
        ImVec2 uv_min = ImVec2(0.0f, 0.0f);  // Top-left
        ImVec2 uv_max = ImVec2(1.0f, 1.0f);  // Lower-right
                                             //
        if(ImGui::BeginItemTooltip())
        {
            float regionSize = 32.0f;  // size of region to show in zoom (in pixels of the image shown in the ui, not the original size of the image)
            ImVec2 mousePos  = ImGui::GetMousePos();
            float regionX    = mousePos.x - pos.x - regionSize * 0.5f;
            float regionY    = mousePos.y - pos.y - regionSize * 0.5f;
            float zoom       = 4.0f;
            if(regionX < 0.0f)
            {
                regionX = 0.0f;
            }
            else if(regionX > width - regionSize)
            {
                regionX = width - regionSize;
            }
            if(regionY < 0.0f)
            {
                regionY = 0.0f;
            }
            else if(regionY > height - regionSize)
            {
                regionY = height - regionSize;
            }

            float widthRatio  = m_image->GetWidth() / static_cast<float>(width);
            float heightRatio = m_image->GetHeight() / static_cast<float>(height);

            ImGui::Text("Min: (%.2f, %.2f)", regionX * widthRatio, regionY * heightRatio);
            ImGui::Text("Max: (%.2f, %.2f)", (regionX + regionSize) * widthRatio, (regionY + regionSize) * heightRatio);
            ImVec2 uv0 = ImVec2((regionX) / width, (regionY) / height);
            ImVec2 uv1 = ImVec2((regionX + regionSize) / width, (regionY + regionSize) / height);
            ImGui::Image(m_desc, ImVec2(regionSize * zoom, regionSize * zoom), uv0, uv1);
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }

private:
    Image* m_image;
    VkDescriptorSet m_desc;
};

class DebugUIWindow
{
public:
    DebugUIWindow(const std::string& name = "Debug", bool opened = true)
        : m_debugUI(nullptr),
          m_index(0),
          m_name(name),
          m_opened(opened) {}

    ~DebugUIWindow();

    void AddElement(std::shared_ptr<DebugUIElement> element, uint32_t column = 1)
    {
        while(m_columnHeights.size() < column)
        {
            m_columnHeights.push_back(0);
        }
        auto it = m_registry.find(element->GetName());
        if(it != m_registry.end())
        {
            LOG_ERROR("DebugUIWindow: {0} already contains an element named: {1}", m_name, element->GetName());
            return;
        }

        std::pair<uint32_t, uint32_t> coords = {m_columnHeights[column - 1]++, column};

        m_elements[coords]             = element;
        m_registry[element->GetName()] = coords;
    }

    std::shared_ptr<DebugUIElement> GetElement(const std::string& name, uint32_t column = 1)
    {
        if(m_columnHeights.size() < column)
        {
            LOG_ERROR("GetElement called on debugUIWindow: {0} with param column: {1}, but the window has {2} columns in total", m_name, column, m_columnHeights.size());
            return nullptr;
        }
        auto it = m_registry.find(name);
        if(it != m_registry.end())
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
        if(it != m_registry.end())
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
                ImGui::BeginTable("table1", static_cast<int>(m_columnHeights.size()));

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
    std::map<std::pair<uint32_t, uint32_t>, std::shared_ptr<DebugUIElement>> m_elements;
    std::vector<uint32_t> m_columnHeights;
};
