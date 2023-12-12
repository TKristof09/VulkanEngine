#pragma once

#include <vector>
#include <string>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include "ECS/Core.hpp"
#include "ECS/Entity.hpp"

class Vertex;
class Mesh;
class AssimpImporter
{
public:
    static Entity LoadFile(const std::string& file, ECS* ecs, Entity* parent = nullptr);

private:
    static void ProcessNode(const aiNode* node, const aiScene* scene, ECS* ecs, Entity entity);
    static void LoadMesh(const aiMesh* mesh, const aiScene* scene, Entity entity);

    static Assimp::Importer s_importer;
};
