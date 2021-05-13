#include "ECS/CoreSystems/TransformHierarchySystem.hpp"
#include "ECS/ComponentManager.hpp"
#include "ECS/CoreComponents/Relationship.hpp"
#include "ECS/CoreComponents/Static.hpp"
#include "ECS/Types.hpp"

TransformHierarchySystem::TransformHierarchySystem()
{
	Subscribe(&TransformHierarchySystem::OnTransformAdded);
}

void TransformHierarchySystem::Update(double dt)
{
	for(auto it = m_scene.ecs->componentManager->begin<Relationship>(); it != m_scene.ecs->componentManager->end<Relationship>(); ++it)
	{
		EntityID parentID = it->parent;
		Transform* transform = m_scene.ecs->componentManager->GetComponent<Transform>(it->GetOwner());

		if(parentID != INVALID_ENTITY_ID)
		{
			Transform* parent = m_scene.ecs->componentManager->GetComponent<Transform>(parentID);
			transform->parentTransform = parent->GetTransform();
		}
	}
}

void TransformHierarchySystem::OnTransformAdded(const ComponentAdded<Transform>* event)
{
	// https://skypjack.github.io/2019-08-20-ecs-baf-part-4-insights/
	if(!m_scene.ecs->componentManager->HasComponent<Static>(event->entity))
		m_scene.ecs->componentManager->Sort<Transform>(
				[&](Transform* lhs, Transform* rhs)
				{
					Relationship* rhsRelationship = m_scene.ecs->componentManager->GetComponent<Relationship>(rhs->GetOwner());
					Relationship* lhsRelationship = m_scene.ecs->componentManager->GetComponent<Relationship>(lhs->GetOwner());

					EntityID lhsEntity = lhs->GetOwner();
					EntityID rhsEntity = rhs->GetOwner();

					return     rhsRelationship->parent == lhsEntity
					|| lhsRelationship->nextSibling == rhsEntity
					|| (!(lhsRelationship->parent == rhsEntity || rhsRelationship->nextSibling == lhsEntity) && (lhsRelationship->parent < rhsRelationship->parent	|| (lhsRelationship->parent == rhsRelationship->parent && lhsRelationship  < rhsRelationship)));
				});
}
