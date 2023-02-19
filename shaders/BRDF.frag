/////////////////////////////////////////////////////////////////////////
// Pixel shader for lighting
////////////////////////////////////////////////////////////////////////
#version 330

out vec4 FragColor;

// These definitions agree with the ObjectIds enum in scene.h
const int     nullId	= 0;
const int     skyId	= 1;
const int     seaId	= 2;
const int     groundId	= 3;
const int     roomId	= 4;
const int     boxId	= 5;
const int     frameId	= 6;
const int     lPicId	= 7;
const int     rPicId	= 8;
const int     teapotId	= 9;
const int     spheresId	= 10;
const int     floorId	= 11;

float pi = 3.14159;
float pi2 = 2*pi;

in vec3 eyePos;

// lighting data
uniform vec3  lightPos;
uniform vec3  lightVal;
uniform vec3  lightAmb;
uniform float lightRange;

uniform bool isLight;
uniform bool debugLocalLight;

vec3 BRDF(vec3 Pos, vec3 N, vec3 Kd, vec3 Ks, float alpha)
{
    vec3 L = normalize(lightPos - Pos);
    vec3 V = normalize(eyePos   - Pos);
    vec3 H = normalize(L + V);  
    vec3 Ii = lightVal;
    vec3 Ia = lightAmb;


    float dist = distance(lightPos, Pos);
    if(isLight) {
      if(dist <= lightRange && dist > 0.001) {
        float attenuation = (1.0 / (dist * dist)  - 1.0 / (lightRange * lightRange));
        Ii *= attenuation;
      }
      /*else {
        if(debugLocalLight) 
          return Ii;
        else
          return vec3(0.0);
      }*/
    }
    
    N = normalize(N);
    
    vec3  BRDF_part;
    vec3  F;
    float D  = 0.0;
    float G1 = 0.0;
    float G2 = 0.0;
    float LdotN;
    float HdotN;
    float VdotN = dot(V, N);
    
    LdotN = max(dot(L, N), 0.0);
    HdotN = max(dot(H, N), 0.0);
    VdotN = dot(V, N);
    
    // F term
    F = Ks + (1 - Ks) * pow((1 - dot(L,H)), 5);
    
    // D term
    D = (alpha + 2) / (2 * pi) * pow(HdotN, alpha);

    // G term
    float a =  pow(alpha / 2.0 + 1.0, 0.5)  / (pow(1.0 - LdotN * LdotN, 0.5) / LdotN);
    if(a != 0 && a < 1.6)
        G1 = 3.535 * a + 2.181 * a * a / (1.0 + 2.276 * a + 2.577 * a * a);
    else
        G1 = 1.0;

    a = pow(alpha / 2.0 + 1.0, 0.5) / (pow(1.0 - VdotN * VdotN, 0.5) / VdotN);
    if(a != 0 && a < 1.6)
        G2 = 3.535 * a + 2.181 * a * a / (1.0 + 2.276 * a + 2.577 * a * a);
    else
        G2 = 1.0;

    if(G1 > 1.0) G1 = 1.0;
    if(G2 > 1.0) G2 = 1.0;

    BRDF_part =  (Kd / pi) + (F * G1 * G2 * D / (4 * LdotN * VdotN));

    return Ia * Kd + Ii * LdotN * BRDF_part;
}
