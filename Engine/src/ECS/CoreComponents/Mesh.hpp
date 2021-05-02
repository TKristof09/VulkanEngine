#pragma once

#include "ECS/Component.hpp"
#include <vector>
#include <string.h>
#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "Rendering/Buffer.hpp"
#include "Rendering/CommandBuffer.hpp"

struct Vertex
{
	glm::vec3 pos;
	glm::vec2 texCoord;
};

struct Mesh : public Component<Mesh>
{
	Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices):
		vertices(vertices),
		indices(indices) {};
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

};


static VkVertexInputBindingDescription GetVertexBindingDescription()
{
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride  = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bindingDescription;
}

static std::array<VkVertexInputAttributeDescription, 2> GetVertexAttributeDescriptions()
{
	std::array<VkVertexInputAttributeDescription, 2> attribDescriptions = {};

	attribDescriptions[0].binding = 0;
	attribDescriptions[0].location = 0;
	attribDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDescriptions[0].offset = offsetof(Vertex, pos);

	attribDescriptions[1].binding = 0;
	attribDescriptions[1].location = 1;
	attribDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
	attribDescriptions[1].offset = offsetof(Vertex, texCoord);

	return attribDescriptions;
}

