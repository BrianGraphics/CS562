#version 330

in vec4 position;

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
    gl_FragData[0] = vec4(d, d*d, d*d*d, d*d*d*d);  
}
