#pragma once

#include "ECS/Component.hpp"
#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/array.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

struct Vertex
{
	glm::vec3 pos;
	glm::vec2 texCoord;
};

struct Mesh : public Component<Mesh>
{
	Mesh(const eastl::vector<Vertex>& vertices, const eastl::vector<uint32_t>& indices):
		vertices(vertices),
		indices(indices) {};
	eastl::vector<Vertex> vertices;
	eastl::vector<uint32_t> indices;
};


static VkVertexInputBindingDescription GetVertexBindingDescription()
{
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride  = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bindingDescription;
}

static eastl::array<VkVertexInputAttributeDescription, 2> GetVertexAttributeDescriptions()
{
	eastl::array<VkVertexInputAttributeDescription, 2> attribDescriptions = {};

	attribDescriptions[0].binding = 0;
	attribDescriptions[0].location = 0;
	attribDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDescriptions[0].offset = offsetof(Vertex, pos);

	attribDescriptions[1].binding = 0;
	attribDescriptions[1].location = 2;
	attribDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
	attribDescriptions[1].offset = offsetof(Vertex, texCoord);

	return attribDescriptions;
}

