#version 330

in vec4 vertex;

void BRDF();

void main()
{
    gl_Position = vertex;
    BRDF();
}
