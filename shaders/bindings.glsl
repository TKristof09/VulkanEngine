// bindings that are used in every shader
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
//layout (set = 0, binding = 0) uniform sampler2D textures[];
layout(buffer_reference, buffer_reference_align=4) readonly buffer Transforms {
    mat4 m[];
};
layout (push_constant) uniform PC
{
    mat4 viewProj;

    uint64_t shaderDataPtr;
    Transforms transformsPtr; // accessed with objectId
    uint64_t drawIdToObjectIdPtr; // used to access transforms, converts glDrawId to objectId
    uint64_t materialsPtr; // accessed with glDrawId
};
