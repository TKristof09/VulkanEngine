#pragma once
#include "ECS/Component.hpp"
#include <glm/glm.hpp>

struct Material : public Component<Material>
{
    std::string shaderName;  // each shader in the pipeline must have the same name only with different file extensions (except maybe compute shaders when we support those)
    uint32_t priority = std::numeric_limits<uint32_t>::max();
    std::unordered_map<std::string, std::string> textures;
    // std::unordered_map<std::string, std::any> values;

    glm::vec3 albedo = glm::vec3(1.0f, 0.0f, 1.0f);
    float roughness  = 1.0f;
    float metallic   = 0.0;

    uint32_t _ubSlot;       // internal
    uint32_t _textureSlot;  // internal
};

struct ShaderMaterial
{
    uint32_t albedoTexture;
    uint32_t normalTexture;
    uint32_t roughnessTexture;
    uint32_t metallicTexture;

    glm::vec3 albedo;
    float roughness;
    float metallic;
};
