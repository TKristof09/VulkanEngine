#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include "CommandBuffer.hpp"
#include "Buffer.hpp"

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;


	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride  = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attribDescriptions = {};

		attribDescriptions[0].binding = 0;
		attribDescriptions[0].location = 0;
		attribDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribDescriptions[0].offset = offsetof(Vertex, pos);

		attribDescriptions[1].binding = 0;
		attribDescriptions[1].location = 1;
		attribDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribDescriptions[1].offset = offsetof(Vertex, color);

		attribDescriptions[2].binding = 0;
		attribDescriptions[2].location = 2;
		attribDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attribDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attribDescriptions;
	}
	bool operator==(const Vertex& other) const
	{
		return pos == other.pos && color == other.color && texCoord == other.texCoord;
	}
};
namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}


class Model
{
	public:
		Model(const std::string& file, VkPhysicalDevice gpu, VkDevice device, VkCommandPool commandPool, VkQueue queue, uint32_t numCommandBuffers);
		~Model();
		VkCommandBuffer GetCommandBuffer(uint32_t index) const { return m_commandBuffers[index]->GetCommandBuffer(); };
		void SetupCommandBuffer(uint32_t index, VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkRenderPass renderPass, VkFramebuffer framebuffer, VkDescriptorSet descriptorSet, uint32_t pushConstantsSize = 0, void* pushConstantsData = nullptr);
	private:
		void LoadModel(const std::string& file);
		void CreateBuffers(VkPhysicalDevice gpu, VkDevice device, VkCommandPool commandPool, VkQueue queue);


		std::vector<Vertex> m_vertices;
		std::vector<uint32_t> m_indices;
		std::vector<std::unique_ptr<CommandBuffer>> m_commandBuffers;

		std::unique_ptr<Buffer> m_vertexBuffer;
		std::unique_ptr<Buffer> m_indexBuffer;
};
