#pragma once

#include <vector>
#include <string.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include "ECS/ECSEngine.hpp"
#include "ECS/Types.hpp"

class Vertex;
class Mesh;
class AssimpImporter
{
public:
	EntityID LoadFile(const std::string& file, ECSEngine* ecsEngine);

private:
	void ProcessNode(const aiNode* node, const aiScene* scene, ECSEngine* ecsEngine, EntityID entity);
	void LoadMesh(const aiMesh* mesh, EntityID entity, ECSEngine* ecsEngine);
};
