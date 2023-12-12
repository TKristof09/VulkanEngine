#pragma once

#include <vector>
#include <glm/glm.hpp>

struct Vertex
{
    glm::vec3 pos;
    glm::vec2 texCoord;
    glm::vec3 normal;
};

struct Mesh
{
    Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) : vertices(vertices),
                                                                                      indices(indices){};
    Mesh(){};
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};
