#include "TransformSystem.hpp"
#include <glm/gtx/quaternion.hpp>

void TransformSystem::Initialize()
{
    // Pre render phase because we want to calculate the world transforms at the end after all gameplay logic is done, right before rendering
    StartSystemBuilder(SystemPhase::PreRender, CalculateWorldTransforms)
        .term_at(2)
        .parent()
        .cascade()   // second term comes from parent in breadth first order
        .optional()  // to match root level entities as well

        .term_at(3)
        .write()  // third term will be set by this system
        .each(CalculateWorldTransforms);
}

void TransformSystem::CalculateWorldTransforms(const Transform& transform, const InternalTransform* parentTransform, InternalTransform& internalTransform)
{
    glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), transform.pos) * glm::toMat4(transform.rot) * glm::scale(glm::mat4(1.0f), transform.scale);

    if(parentTransform == nullptr)
        internalTransform.worldTransform = localTransform;
    else
        internalTransform.worldTransform = parentTransform->worldTransform * localTransform;
}
