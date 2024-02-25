#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera
{
    float fovRadians;
    float aspect;
    float zNear;

    Camera(float fovDeg, float aspect, float zNear) : fovRadians(glm::radians(fovDeg)), aspect(aspect), zNear(zNear) {}
};

struct MainCameraData
{
    glm::mat4 viewProj;
    glm::mat4 view;
    glm::vec3 pos;
    float zNear;
    glm::vec2 clipToViewSpaceConsts;  //(aspect * tan(fov / 2), tan(fov / 2))
};

inline glm::mat4 GetProjection(const Camera& camera)
{
    float f = 1.0f / tan(camera.fovRadians / 2.0f);
    // f / aspect 0.0  0.0 0.0
    // 0.0        f    0.0 0.0
    // 0.0        0.0  0.0 zNear
    // 0.0        0.0 -1.0 0.0
    // contructor is column major so the way this matrix is formatted in code is actually the transpose of the matrix
    return glm::mat4(
        f / camera.aspect, 0.0f, 0.0f, 0.0f,
        0.0f, f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, -1.0f,
        0.0f, 0.0f, camera.zNear, 0.0f);
    // return glm::perspective(glm::radians(fovDegrees), aspect, zNear, zFar);
}
