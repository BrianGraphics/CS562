#version 460
in vec4 position;
out vec4 FragColor;

uniform float minDist;
uniform float maxDist;

void main()
{
    // reletive depth calculation
    float z  = position.w;
    float z1 = maxDist;
    float z0 = minDist;
    float d  = (z - z0) / (z1 - z0);
    if(d < 0.0) d = 0.0;


    // output msm
    FragColor = vec4(d, d*d, d*d*d, d*d*d*d);  
}
