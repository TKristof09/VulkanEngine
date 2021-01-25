#pragma once
#include "ECS/Component.hpp"
#include "UniformBuffer.hpp"
#include "Texture.hpp"
struct Material : public Component<Material>
{
	std::string shaderName; // each shader in the pipeline must have the same name only with different file extensions (except maybe compute shaders when we support those)
	std::unordered_map<std::string, Texture> textures;
	std::unordered_map<std::string, UniformBuffer> uniformBuffers;
};
