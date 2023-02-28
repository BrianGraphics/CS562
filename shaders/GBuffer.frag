#version 330

out vec4 FragColor[];

in vec3 normalVec;
in vec2 texCoord;
in vec4 worldPos;

uniform vec3 diffuse;
uniform vec3 specular;
uniform float shininess;

void main()
{
    FragColor[0]     = worldPos;
    FragColor[1].xyz = normalVec;
    FragColor[2].xyz = diffuse;
    FragColor[3]     = vec4(specular, shininess);
}
