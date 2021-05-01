#include "ECS/SceneSerializer.hpp"

#include "glm/glm.hpp"
std::unordered_map<ComponentTypeID, SerializeFunction> SceneSerializer::m_serializeFunctions = {};
std::unordered_map<std::string, DeserializeFunction> SceneSerializer::m_deserializeFunctions = {};

namespace YAML {

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if(!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if(!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};

}

YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
	return out;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
	return out;
}
void SceneSerializer::Serialize(const std::string& path)
{
	YAML::Emitter out;

	out << YAML::BeginMap;
	out << YAML::Key << "Scene" << YAML::Value << "Untitled";
	out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
	
	auto entities = ECSEngine::GetInstance().entityManager->GetAllEntities();

	for(auto entity : entities)
	{
		SerializeEntity(out, entity->GetEntityID());
	}

	out << YAML::EndSeq;
	out << YAML::EndMap;

}

void SceneSerializer::Deserialize(const std::string& path)
{
	YAML::Node data = YAML::LoadFile(path);
	if(!data["Scene"])
		return;

	std::string sceneName = data["Scene"].as<std::string>();
	LOG_INFO("Deserializing scene: {0}, from file: {1}", sceneName, path);

	auto entities = data["Entities"];
	if(!entities)
		return;

	
	for(auto entity : entities)
		DeserializeEntity(entity);
}

void SceneSerializer::SerializeEntity(YAML::Emitter& out, EntityID entity)
{
	out << YAML::BeginMap; // Entity
	
	auto components = ECSEngine::GetInstance().componentManager->GetAllComponents(entity);

	for (auto& [typeID, component] : components)
	{
		auto it = m_serializeFunctions.find(typeID);
		if(it == m_serializeFunctions.end())
			LOG_WARN("No serialize function registered for component with typeID: {0}", typeID);
		else
			it->second(out, component);

	}

	out << YAML::EndMap; // Entity

}

void SceneSerializer::DeserializeEntity(const YAML::Node& entityNode)
{
	Entity* entity = ECSEngine::GetInstance().entityManager->GetEntity(ECSEngine::GetInstance().entityManager->CreateEntity()); // TODO

	for(auto component : entityNode)
	{
		auto componentName = component.first.as<std::string>();
		auto it = m_deserializeFunctions.find(componentName);

		if(it == m_deserializeFunctions.end())
			LOG_ERROR("No function for deserializing {0}", componentName);
		else
			it->second(component.second, entity);

	}
	
}
