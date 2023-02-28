#version 330

uniform mat4 WorldView, WorldInverse, WorldProj, ModelTr, NormalTr;

in vec4 vertex;
in vec3 vertexNormal;

out vec3 normalVec;

void BRDF();

void main()
{
    gl_Position = WorldProj*WorldView*ModelTr*vertex;

    normalVec = vertexNormal*mat3(NormalTr); 
}
