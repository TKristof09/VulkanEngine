#include "Utils/AssimpImporter.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <assimp/postprocess.h>

#include "ECS/CoreComponents/Transform.hpp"
#include "ECS/CoreComponents/Mesh.hpp"
#include "ECS/EntityManager.hpp"

bool AssimpImporter::LoadFile(const std::string& file, ECSEngine* ecsEngine)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(file.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);


	if(!scene)
	{
		std::cerr << importer.GetErrorString() << std::endl;
		return false;
	}

	ProcessNode(scene->mRootNode, scene, ecsEngine, ecsEngine->entityManager->CreateEntity());
	return true;
}

glm::vec3 ToGLM(const aiVector3D& v) { return glm::vec3(v.x, v.y, v.z); }
glm::vec2 ToGLM(const aiVector2D& v) { return glm::vec2(v.x, v.y); }
glm::quat ToGLM(const aiQuaternion& q) { return glm::quat(q.w, q.x, q.y, q.z); }

void AssimpImporter::ProcessNode(const aiNode* node, const aiScene* scene, ECSEngine* ecsEngine, EntityID entity)
{
	aiVector3D pos, scale;
	aiQuaternion rot;
	node->mTransformation.Decompose(scale, rot, pos);

	Transform transform;
	transform.lPosition = ToGLM(pos);
	transform.lRotation = ToGLM(rot);
	transform.lScale    = ToGLM(scale);

	for (int i = 0; i < node->mNumMeshes; ++i)
	{
		LoadMesh(scene->mMeshes[node->mMeshes[i]], entity, ecsEngine);
	}

	for (int i = 0; i < node->mNumChildren; ++i)
	{
		ProcessNode(node->mChildren[i], scene, ecsEngine, ecsEngine->entityManager->CreateChild(entity));
	}
}

void AssimpImporter::LoadMesh(const aiMesh* mesh, EntityID entity, ECSEngine* ecsEngine)
{

	std::vector<Vertex> vertices;
	vertices.resize(mesh->mNumVertices);
	std::vector<uint32_t> indices;
	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;

		vertex.pos = ToGLM(mesh->mVertices[i]);

		if(mesh->HasTextureCoords(0))
		{
			vertex.texCoord = ToGLM(mesh->mTextureCoords[0][i]);
		}
		else
		{
			vertex.texCoord = glm::vec2(0);
		}

		//vertex.normal = ToGLM(mesh->mNormals[i]);

		//vertex.tangent = ToGLM(mesh->mTangents[i]);

		vertices[i] = vertex;
	}
	for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
	{
		aiFace face = mesh->mFaces[i];
		for (int j = 0; j < face.mNumIndices; ++j)
		{
			indices.push_back(face.mIndices[j]);
		}
	}
	ecsEngine->componentManager->AddComponent<Mesh>(entity, vertices, indices );
}
