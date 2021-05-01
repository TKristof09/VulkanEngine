#pragma once

#include <vector>
#include <string.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include "ECS/ECSEngine.hpp"
#include "ECS/Entity.hpp"
#include "ECS/Types.hpp"

class Vertex;
class Mesh;
class AssimpImporter
{
public:
	Entity* LoadFile(const std::string& file, ECSEngine* ecsEngine);

private:
	void ProcessNode(const aiNode* node, const aiScene* scene, ECSEngine* ecsEngine, Entity* entity);
	void LoadMesh(const aiMesh* mesh, Entity* entity);
};
