#version 330

out vec4 FragColor[];

in vec3 normalVec;
in vec4 worldPos;

uniform vec3 diffuse;
uniform vec3 specular;
uniform float shininess;

uniform mat4 WorldInverse;

#define pi 3.1415926538
uniform int objectId;
uniform sampler2D skyTex;

void main()
{
    FragColor[0]     = worldPos;
    FragColor[1].xyz = normalVec;
    FragColor[2].xyz = diffuse;
    FragColor[3]     = vec4(specular, shininess);

    // skydome
    if(objectId == 1) {
        vec3 V = normalize((WorldInverse * vec4(0, 0, 0, 1)).xyz - worldPos.xyz);
        vec2 tex_uv = vec2((- 1.0) * atan(V.y,V.x)/(2*pi), acos(V.z)/pi); 
        FragColor[2].xyz = texture2D(skyTex, tex_uv).xyz; 
        FragColor[3]     = vec4(specular, 6969);
    }
}
