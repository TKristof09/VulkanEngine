#pragma once
#include "ECS/ECS.hpp"
#include "yaml-cpp/yaml.h"

typedef void (*SerializeFunction)(YAML::Emitter& out, IComponent* component);
typedef void (*DeserializeFunction)(const YAML::Node& node, Entity* entity);

class SceneSerializer
{
public:

	static void Serialize(const std::string& path, ECSEngine* ecs);
	static Scene Deserialize(const std::string& path);

private:
	static void SerializeEntity(YAML::Emitter& out, EntityID entity, ECSEngine* ecs);
	static void DeserializeEntity(const YAML::Node& entityNode, ECSEngine* ecs);

	static std::unordered_map<ComponentTypeID, SerializeFunction> m_serializeFunctions;
	static std::unordered_map<std::string, DeserializeFunction> m_deserializeFunctions;

};