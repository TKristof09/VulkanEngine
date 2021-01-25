#pragma once
#include <glm/glm.hpp>

struct Color
{
    Color():
        r(0),
        g(0),
        b(0),
        a(0) {}

    Color(float R, float G, float B) :
        r(R / 255.0f),
        g(G / 255.0f),
        b(B / 255.0f),
        a(1) {}

    Color(float R, float G, float B, float A) :
        r(R / 255.0f),
        g(G / 255.0f),
        b(B / 255.0f),
        a(A) {}

    Color(uint32_t HEX_RGB, float alpha) :
        r(((HEX_RGB & 0xff0000) >> 16) / 255.0f),
        g(((HEX_RGB & 0x00ff00) >> 8) / 255.0f),
        b((HEX_RGB & 0x0000ff) / 255.0f),
        a(alpha) {}

	Color(uint32_t HEX_RGBA):
		r(((HEX_RGBA & 0xff000000) >> 24) / 255.0f),
        g(((HEX_RGBA & 0x00ff0000) >> 16) / 255.0f),
        b(((HEX_RGBA & 0x0000ff00) >> 8) / 255.0f),
		a((HEX_RGBA & 0x000000ff) / 255.0f) {}


	glm::vec3 ToVector3() const { return glm::vec3(r, g, b); }
    glm::vec4 ToVector4() const { return glm::vec4(r, g, b, a); }

    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Black;
    static const Color White;

    float r, g, b, a;
};

const Color Color::Red(0xff0000ff);
const Color Color::Green(0x00ff00ff);
const Color Color::Blue(0x0000ffff);
const Color Color::Black(0x000000ff);
const Color Color::White(0xffffffff);
