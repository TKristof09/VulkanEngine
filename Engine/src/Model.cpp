#include "Model.hpp"
#include <cstddef>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <unordered_map>
//#include <assimp/scene.h>
//#include <assimp/Importer.hpp>
//#include <assimp/postprocess.h>
Model::Model(const std::string& file, VkPhysicalDevice gpu, VkDevice device, VkCommandPool commandPool, VkQueue queue, uint32_t numCommandBuffers)
{
	m_commandBuffers.resize(numCommandBuffers);
	for(size_t i = 0; i < numCommandBuffers; i++)
	{
		m_commandBuffers[i] = std::make_unique<CommandBuffer>(device, commandPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
	}

	LoadModel(file);
	CreateBuffers(gpu, device, commandPool, queue);
	std::cout << m_vertices.size() << std::endl;
}

Model::~Model()
{

}

void Model::LoadModel(const std::string& file)
{
	
	tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file.c_str())) {
        throw std::runtime_error(warn + err);
    }

	std::unordered_map<Vertex, uint32_t> uniqueVertices = {};
	for(const auto& shape : shapes)
	{
		for(const auto& index : shape.mesh.indices)
		{
			Vertex vertex = {};
			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.texCoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			vertex.color = {1.0f, 1.0f, 1.0f};
	
			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
				m_vertices.push_back(vertex);
			}

			m_indices.push_back(uniqueVertices[vertex]);
		}

	}
}

void Model::CreateBuffers(VkPhysicalDevice gpu, VkDevice device, VkCommandPool commandPool, VkQueue queue)
{
	 VkDeviceSize bufferSize = sizeof(m_vertices[0]) * m_vertices.size();

	Buffer stagingVertexBuffer(gpu, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	stagingVertexBuffer.Fill((void*)m_vertices.data(), bufferSize);

	m_vertexBuffer = std::make_unique<Buffer>(gpu, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	stagingVertexBuffer.Copy(m_vertexBuffer.get(), bufferSize, queue, commandPool);


	bufferSize = sizeof(m_indices[0]) * m_indices.size();
	Buffer stagingIndexBuffer(gpu, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	stagingIndexBuffer.Fill((void*)m_indices.data(), bufferSize);

	m_indexBuffer = std::make_unique<Buffer>(gpu, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	stagingIndexBuffer.Copy(m_indexBuffer.get(), bufferSize, queue, commandPool);

}
void Model::SetupCommandBuffer(uint32_t index, VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkRenderPass renderPass, VkFramebuffer framebuffer, VkDescriptorSet descriptorSet)
{
	VkCommandBufferInheritanceInfo inheritanceInfo = {};
	inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceInfo.renderPass = renderPass;
	inheritanceInfo.subpass = 0;
	inheritanceInfo.framebuffer = framebuffer;
	m_commandBuffers[index]->Begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, inheritanceInfo);

	vkCmdBindPipeline(m_commandBuffers[index]->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	
	m_vertexBuffer->Bind(*m_commandBuffers[index]);
	m_indexBuffer->Bind(*m_commandBuffers[index]);
	
	vkCmdBindDescriptorSets(m_commandBuffers[index]->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	vkCmdDrawIndexed(m_commandBuffers[index]->GetCommandBuffer(), static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);

	m_commandBuffers[index]->End();
}
