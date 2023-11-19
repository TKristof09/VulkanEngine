#include "Utils/AssimpImporter.hpp"
#include <assimp/material.h>
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

Entity* AssimpImporter::LoadFile(const std::string& file, ECSEngine* ecsEngine, Entity* parent)
{
    auto* assimpLogger = Assimp::DefaultLogger::create("DebugTools/AssimpLogger.txt", Assimp::Logger::VERBOSE);
    assimpLogger->info("Test info call");


    s_importer.SetPropertyBool("GLOB_MEASURE_TIME", true);
    const aiScene* scene = s_importer.ReadFile(file.c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph);


    if(!scene)
    {
        LOG_ERROR(s_importer.GetErrorString());
        return nullptr;
    }
    Entity* rootEntity;
    if(!parent)
    {
        rootEntity = ecsEngine->entityManager->CreateEntity();
    }
    else
    {
        rootEntity = ecsEngine->entityManager->CreateChild(parent);
    }

    ProcessNode(scene->mRootNode, scene, ecsEngine, rootEntity);
    LOG_TRACE("Loaded {0}", file);

    Assimp::DefaultLogger::kill();

    return rootEntity;
}

glm::vec3 ToGLM(const aiVector3D& v) { return {v.x, v.y, v.z}; }
glm::vec2 ToGLM(const aiVector2D& v) { return {v.x, v.y}; }
glm::quat ToGLM(const aiQuaternion& q) { return {q.w, q.x, q.y, q.z}; }

void AssimpImporter::ProcessNode(const aiNode* node, const aiScene* scene, ECSEngine* ecsEngine, Entity* entity)
{
    aiVector3D pos;
    aiVector3D scale;
    aiQuaternion rot;
    node->mTransformation.Decompose(scale, rot, pos);

    Transform* transform = entity->GetComponent<Transform>();
    transform->pos       = ToGLM(pos);
    transform->rot       = ToGLM(rot);
    transform->scale     = ToGLM(scale);

    entity->AddComponent<NameTag>()->name = node->mName.C_Str();


    if(node->mNumMeshes == 1)
    {
        LoadMesh(scene->mMeshes[node->mMeshes[0]], scene, entity);
    }
    else
    {
        for(int i = 0; i < node->mNumMeshes; ++i)
        {
            LoadMesh(scene->mMeshes[node->mMeshes[i]], scene, ecsEngine->entityManager->CreateChild(entity));
        }
    }

    for(int i = 0; i < node->mNumChildren; ++i)
    {
        ProcessNode(node->mChildren[i], scene, ecsEngine, ecsEngine->entityManager->CreateChild(entity));
    }
}

void AssimpImporter::LoadMesh(const aiMesh* mesh, const aiScene* scene, Entity* entity)
{
    std::vector<Vertex> vertices;
    vertices.resize(mesh->mNumVertices);
    std::vector<uint32_t> indices;
    for(unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex{};

        vertex.pos = ToGLM(mesh->mVertices[i]);

        if(mesh->HasTextureCoords(0))
        {
            vertex.texCoord   = ToGLM(mesh->mTextureCoords[0][i]);
            vertex.texCoord.y = 1 - vertex.texCoord.y;  // need this fix because of vulkan's weird coordinates
        }
        else
        {
            vertex.texCoord = glm::vec2(0);
        }

        vertex.normal = ToGLM(mesh->mNormals[i]);


        vertices[i] = vertex;
    }
    for(unsigned int i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace face = mesh->mFaces[i];
        for(int j = 0; j < face.mNumIndices; ++j)
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    Material* mat2    = entity->AddComponent<Material>();
    mat2->shaderName  = "forwardplus";
    aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];
    aiString tempPath;
    if(aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &tempPath) == aiReturn_SUCCESS)
    {
        std::string texturePath = tempPath.C_Str();
        /* texturePath        = "./models/" + texturePath;
        auto it                 = texturePath.find('\\');
        texturePath.replace(it, 1, "/", 1);*/
        TextureManager::LoadTexture(texturePath);
        mat2->textures["albedo"] = texturePath;
    }

    if(aiMat->GetTexture(aiTextureType_NORMALS, 0, &tempPath) == aiReturn_SUCCESS)
    {
        std::string texturePath = tempPath.C_Str();
        /* texturePath        = "./models/" + texturePath;
        auto it                 = texturePath.find("\\");
        texturePath.replace(it, 1, "/", 1);*/
        TextureManager::LoadTexture(texturePath);
        mat2->textures["normal"] = texturePath;
    }

    // for some reason the roughness texture gets put into aiTextureType_SHININESS for fbx files
    if(aiMat->GetTexture(aiTextureType_SHININESS, 0, &tempPath) == aiReturn_SUCCESS)
    {
        std::string texturePath = tempPath.C_Str();
        /* texturePath        = "./models/" + texturePath;
        auto it                 = texturePath.find("\\");
        texturePath.replace(it, 1, "/", 1);*/
        TextureManager::LoadTexture(texturePath);
        mat2->textures["roughness"] = texturePath;
    }

    if(aiMat->GetTexture(aiTextureType_METALNESS, 0, &tempPath) == aiReturn_SUCCESS)
    {
        std::string texturePath = tempPath.C_Str();
        /* texturePath        = "./models/" + texturePath;
        auto it                 = texturePath.find("\\");
        texturePath.replace(it, 1, "/", 1);*/
        TextureManager::LoadTexture(texturePath);
        mat2->textures["metallic"] = texturePath;
    }


    if(mesh->mName.length > 0)
        entity->AddComponent<NameTag>()->name = mesh->mName.C_Str();
    entity->AddComponent<Mesh>(vertices, indices);
}
