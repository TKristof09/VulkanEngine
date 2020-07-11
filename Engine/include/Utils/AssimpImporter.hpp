#pragma once

#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include "ECS/ECSEngine.hpp"
#include "ECS/Types.hpp"

class Vertex;
class Mesh;
class AssimpImporter
{
public:
	bool LoadFile(const eastl::string& file, ECSEngine* ecsEngine);

private:
	void ProcessNode(const aiNode* node, const aiScene* scene, ECSEngine* ecsEngine, EntityID entity);
	void LoadMesh(const aiMesh* mesh, EntityID entity, ECSEngine* ecsEngine);
};
