#include "Utils/AssimpImporter.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <assimp/postprocess.h>
#include <assimp/DefaultLogger.hpp>

#include "ECS/CoreComponents/Transform.hpp"
#include "ECS/CoreComponents/Mesh.hpp"
#include "ECS/EntityManager.hpp"
#include "Rendering/TextureManager.hpp"
#include "ECS/CoreComponents/Material.hpp"

Assimp::Importer AssimpImporter::s_importer = {};

Entity* AssimpImporter::LoadFile(const std::string& file, ECSEngine* ecsEngine)
{

	auto* assimpLogger = Assimp::DefaultLogger::create("DebugTools/AssimpLogger.txt", Assimp::Logger::VERBOSE);
	assimpLogger->info("Test info call");


	s_importer.SetPropertyBool("GLOB_MEASURE_TIME", true);
	const aiScene* scene = s_importer.ReadFile(file.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);


	if(!scene)
	{
		LOG_ERROR(s_importer.GetErrorString());
		return nullptr;
	}

	Entity* rootEntity = ecsEngine->entityManager->CreateEntity();
	ProcessNode(scene->mRootNode, scene, ecsEngine, rootEntity);
	LOG_TRACE("Loaded {0}", file);

	Assimp::DefaultLogger::kill();

	return rootEntity;
}

glm::vec3 ToGLM(const aiVector3D& v) { return glm::vec3(v.x, v.y, v.z); }
glm::vec2 ToGLM(const aiVector2D& v) { return glm::vec2(v.x, v.y); }
glm::quat ToGLM(const aiQuaternion& q) { return glm::quat(q.w, q.x, q.y, q.z); }

void AssimpImporter::ProcessNode(const aiNode* node, const aiScene* scene, ECSEngine* ecsEngine, Entity* entity)
{

	aiVector3D pos, scale;
	aiQuaternion rot;
	node->mTransformation.Decompose(scale, rot, pos);

	Transform*  transform = entity->GetComponent<Transform>();
	transform->pos = ToGLM(pos);
	transform->rot = ToGLM(rot);
	transform->scale    = ToGLM(scale);



	for (int i = 0; i < node->mNumMeshes; ++i)
	{
		LoadMesh(scene->mMeshes[node->mMeshes[i]], ecsEngine->entityManager->CreateChild(entity));
	}

	for (int i = 0; i < node->mNumChildren; ++i)
	{
		ProcessNode(node->mChildren[i], scene, ecsEngine, ecsEngine->entityManager->CreateChild(entity));
	}
}

void AssimpImporter::LoadMesh(const aiMesh* mesh, Entity* entity)
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
			vertex.texCoord.y = 1 - vertex.texCoord.y; // need this fix because of vulkan's weird coordinates
		}
		else
		{
			vertex.texCoord = glm::vec2(0);
		}

		vertex.normal = ToGLM(mesh->mNormals[i]);

		vertex.tangent = ToGLM(mesh->mTangents[i]);

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
	entity->AddComponent<Mesh>(vertices, indices );
	TextureManager::LoadTexture("./textures/uv_checker.png");
	TextureManager::LoadTexture("./textures/normal.jpg");
	Material* mat2 = entity->AddComponent<Material>();
	mat2->shaderName = "forwardplus";
	mat2->textures["albedo"] = "./textures/uv_checker.png";
	mat2->textures["normal"] = "./textures/normal.jpg";
}
