#pragma once
#include "glm/glm.hpp"

struct BoundingBox
{
    // TODO: only using vec4 for alignment, change to vec3 when i feel like dealing with alignment stuff
    glm::vec4 min{0};
    glm::vec4 max{0};

    BoundingBox(const glm::vec3& min, const glm::vec3& max) : min({min, 0}), max({max, 0}) {}
};
