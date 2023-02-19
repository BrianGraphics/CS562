#version 330

out vec3 eyePos;

uniform mat4 WorldInverse;

void BRDF()
{
    eyePos = (WorldInverse * vec4(0, 0, 0, 1)).xyz;
}
