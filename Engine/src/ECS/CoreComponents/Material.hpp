#pragma once
#include "ECS/Component.hpp"
#include "Rendering/Texture.hpp"
struct Material : public Component<Material>
{
	std::string shaderName; // each shader in the pipeline must have the same name only with different file extensions (except maybe compute shaders when we support those)
	uint32_t priority = std::numeric_limits<uint32_t>::max();
	std::unordered_map<std::string, std::string> textures;
	// std::unordered_map<std::string, std::any> values;

	uint32_t _ubSlot; // internal
	uint32_t _textureSlot; // internal
};
