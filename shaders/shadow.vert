#version 460

uniform mat4 ProjectionMatrix, ViewMatrix, ModelTr;
in vec4 vertex;

out vec4 position;

void main()
{   
    gl_Position = ProjectionMatrix*ViewMatrix*ModelTr*vertex;
    position = gl_Position;
}
