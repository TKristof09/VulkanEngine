#include "ECS/CoreSystems/TransformHierarchySystem.hpp"
#include "ECS/ComponentManager.hpp"
#include "ECS/CoreComponents/Relationship.hpp"
#include "ECS/CoreComponents/Static.hpp"
#include "ECS/Types.hpp"

TransformHierarchySystem::TransformHierarchySystem()
{
	Subscribe(&TransformHierarchySystem::OnTransformAdded);
}

void TransformHierarchySystem::Update(float dt)
{
	for(auto it = m_ecsEngine->componentManager->begin<Relationship>(); it != m_ecsEngine->componentManager->end<Relationship>(); ++it)
	{
		EntityID parentID = it->parent;
		if(parentID == INVALID_ENTITY_ID)
			continue;
		Transform* parentTransform = m_ecsEngine->componentManager->GetComponent<Transform>(parentID);
	
		Transform* transform = m_ecsEngine->componentManager->GetComponent<Transform>(it->GetOwner());

		transform->wPosition = parentTransform->wPosition + parentTransform->wRotation * transform->lPosition;
		transform->wRotation = parentTransform->wRotation * transform->lRotation;
		transform->wScale = parentTransform->wScale * transform->lScale;
	}
}

void TransformHierarchySystem::OnTransformAdded(const ComponentAdded<Transform>* event)
{
	if(!m_ecsEngine->componentManager->HasComponent<Static>(event->entity))
		m_ecsEngine->componentManager->Sort<Transform>(
				[&](Transform* lhs, Transform* rhs)
				{
					Relationship* rhsRelationship = m_ecsEngine->componentManager->GetComponent<Relationship>(rhs->GetOwner());
					Relationship* lhsRelationship = m_ecsEngine->componentManager->GetComponent<Relationship>(lhs->GetOwner());

					EntityID lhsEntity = lhs->GetOwner();
					EntityID rhsEntity = rhs->GetOwner();

					return     rhsRelationship->parent == lhsEntity
							|| lhsRelationship->nextSibling == rhsEntity
							|| (!(lhsRelationship->parent == rhsEntity || rhsRelationship->nextSibling == lhsEntity) && lhsRelationship->parent < rhsRelationship->parent)
							|| (lhsRelationship->parent == rhsRelationship->parent && lhsRelationship  < rhsRelationship);
				});
}
