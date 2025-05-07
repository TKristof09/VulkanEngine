#include "Utils/AssimpImporter.hpp"
#include <assimp/material.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <assimp/postprocess.h>
#include <assimp/DefaultLogger.hpp>

#include "ECS/CoreComponents/BoundingBox.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "ECS/CoreComponents/Mesh.hpp"
#include "Rendering/TextureManager.hpp"
#include "ECS/CoreComponents/Material.hpp"

Assimp::Importer AssimpImporter::s_importer = {};

Entity AssimpImporter::LoadFile(const std::string& file, ECS* ecs, Entity* parent)
{
    auto* assimpLogger = Assimp::DefaultLogger::create("DebugTools/AssimpLogger.txt", Assimp::Logger::VERBOSE);
    assimpLogger->info("Test info call");


    s_importer.SetPropertyBool("GLOB_MEASURE_TIME", true);

    uint32_t flags = static_cast<uint32_t>(aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph | aiProcess_GenBoundingBoxes);

    const aiScene* scene = s_importer.ReadFile(file.c_str(), flags);


    if(!scene)
    {
        LOG_ERROR(s_importer.GetErrorString());
        return {flecs::entity::null()};  // invalid entity
    }
    Entity rootEntity = parent ? ecs->CreateChildEntity(parent, scene->mRootNode->mName.C_Str()) : ecs->CreateEntity(scene->mRootNode->mName.C_Str());

    ProcessNode(scene->mRootNode, scene, ecs, rootEntity);
    LOG_TRACE("Loaded {0}", file);

    Assimp::DefaultLogger::kill();

    return rootEntity;
}

glm::vec3 ToGLM(const aiVector3D& v) { return {v.x, v.y, v.z}; }
glm::vec2 ToGLM(const aiVector2D& v) { return {v.x, v.y}; }
glm::quat ToGLM(const aiQuaternion& q) { return {q.w, q.x, q.y, q.z}; }

void AssimpImporter::ProcessNode(const aiNode* node, const aiScene* scene, ECS* ecs, Entity entity)
{
    aiVector3D pos;
    aiVector3D scale;
    aiQuaternion rot;
    node->mTransformation.Decompose(scale, rot, pos);

    auto tpos   = ToGLM(pos);
    auto trot   = ToGLM(rot);
    auto tscale = ToGLM(scale);
    entity.SetComponent<Transform>({tpos, trot, tscale});

    if(node->mNumMeshes == 1)
    {
        LoadMesh(scene->mMeshes[node->mMeshes[0]], scene, entity);
    }
    else
    {
        for(uint32_t i = 0; i < node->mNumMeshes; ++i)
        {
            auto name = scene->mMeshes[node->mMeshes[i]]->mName.length == 0 ? std::string(node->mName.C_Str()) + std::to_string(i) : scene->mMeshes[node->mMeshes[i]]->mName.C_Str();
            LoadMesh(scene->mMeshes[node->mMeshes[i]], scene, ecs->CreateChildEntity(&entity, name));
        }
    }

    for(uint32_t i = 0; i < node->mNumChildren; ++i)
    {
        ProcessNode(node->mChildren[i], scene, ecs, ecs->CreateChildEntity(&entity, node->mChildren[i]->mName.C_Str()));
    }
}

void AssimpImporter::LoadMesh(const aiMesh* mesh, const aiScene* scene, Entity entity)
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
        for(uint32_t j = 0; j < face.mNumIndices; ++j)
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    Material mat{};
    mat.shaderName    = "forwardplus";
    aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];
    aiString tempPath;
    /* for(int i = 0; i < aiTextureType_UNKNOWN; ++i)
    {
        LOG_INFO("{0} textures for type {1}", aiMat->GetTextureCount((aiTextureType)i), aiTextureTypeToString((aiTextureType)i));
    }*/
    if(aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &tempPath) == aiReturn_SUCCESS)
    {
        std::string texturePath = tempPath.C_Str();
        /* texturePath        = "./models/" + texturePath;
        auto it                 = texturePath.find('\\');
        texturePath.replace(it, 1, "/", 1);*/
        TextureManager::LoadTexture(texturePath);
        mat.textures["albedo"] = texturePath;
    }

    if(aiMat->GetTexture(aiTextureType_NORMALS, 0, &tempPath) == aiReturn_SUCCESS)
    {
        std::string texturePath = tempPath.C_Str();
        /* texturePath        = "./models/" + texturePath;
        auto it                 = texturePath.find("\\");
        texturePath.replace(it, 1, "/", 1);*/
        TextureManager::LoadTexture(texturePath, false);
        mat.textures["normal"] = texturePath;
    }

    // for some reason the roughness texture gets put into aiTextureType_SHININESS for fbx files
    if(aiMat->GetTexture(aiTextureType_SHININESS, 0, &tempPath) == aiReturn_SUCCESS)
    {
        std::string texturePath = tempPath.C_Str();
        /* texturePath        = "./models/" + texturePath;
        auto it                 = texturePath.find("\\");
        texturePath.replace(it, 1, "/", 1);*/
        TextureManager::LoadTexture(texturePath, false);
        mat.textures["roughness"] = texturePath;
    }

    if(aiMat->GetTexture(aiTextureType_METALNESS, 0, &tempPath) == aiReturn_SUCCESS)
    {
        std::string texturePath = tempPath.C_Str();
        /* texturePath        = "./models/" + texturePath;
        auto it                 = texturePath.find("\\");
        texturePath.replace(it, 1, "/", 1);*/
        TextureManager::LoadTexture(texturePath, false);
        mat.textures["metallic"] = texturePath;
    }

    entity.EmplaceComponent<Mesh>(vertices, indices);
    entity.EmplaceComponent<BoundingBox>(ToGLM(mesh->mAABB.mMin), ToGLM(mesh->mAABB.mMax));
    entity.SetComponent<Material>(mat);
}
