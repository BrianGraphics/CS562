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

// for shadow
uniform sampler2D shadowMap;
uniform sampler2D msmH;
uniform sampler2D MSMap;
uniform mat4 ShadowMatrix;

uniform float minDist;
uniform float maxDist;

//   light depth  pixel depth 
float cholesky (vec4 light_depth, float pixel_depth);

vec3 BRDF(vec3 Pos, vec3 N, vec3 Kd, vec3 Ks, float alpha)
{
    vec3 L = normalize(lightPos - Pos);
    vec3 V = normalize(eyePos   - Pos);
    vec3 H = normalize(L + V);  
    vec3 Ii = lightVal;
    vec3 Ia = lightAmb;
    
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
    
    float G = 0.0f;
    float s = 1.0f;
    vec4 shadowCoord = ShadowMatrix  * vec4(Pos, 1.0);
    if(shadowCoord.w > 0){      
        vec2 shadowIndex = shadowCoord.xy / shadowCoord.w;    
        if(shadowIndex.x >= 0 && shadowIndex.x <= 1 && shadowIndex.y >= 0 && shadowIndex.y <= 1) {
          
          float t = shadowCoord.w;                     
          
          // calculate reletive pixel_depth
          float z1 = maxDist;
          float z0 = minDist;
          float pixel_depth  = (t - z0) / (z1 - z0);
          vec4 light_depth = texture2D(MSMap, shadowIndex); 
          
          //if(pixel_depth - 0.005 > light_depth.x) {
          //  float M1 = light_depth.x;
          //  float M2 = light_depth.y;
          //  float var = M2 - M1*M1;                                
          //  s = var/(var + (pixel_depth - M1) * (pixel_depth - M1));                                  
          //}

          G = cholesky(light_depth, pixel_depth);
        }
    }

    //return Ia * Kd + Ii * LdotN * (s * BRDF_part);
    return Ia * Kd + Ii * LdotN * ((1.0 - G) * BRDF_part);
}

float cholesky (vec4 light_depth, float pixel_depth) {
    float alpha = 0.001;
    vec4 new_b = (1 - alpha) * light_depth + alpha * (vec4(0.5));
    
    float m_11 = 1;
    float m_12 = new_b.x;
    float m_13 = new_b.y;
    float m_22 = new_b.y;
    float m_23 = new_b.z;
    float m_33 = new_b.w;
    
    float a = 1.0;
    float b = m_12 / a;
    float c = m_13 / a;
    
    float d = m_22 - b * b;
    if(d > 0.0) {  
      d = sqrt(d); 
    }
    else {
      d = 0.0001;
    }
    
    float e = (m_23 - b * c) / d;
    float f = m_33 - c * c - e * e;
    if(f > 0.0) {  
      f = sqrt(f);
    }
    else {
      f = 0.0001;
    }  
    
    vec3 z = vec3(1.0, pixel_depth, pixel_depth * pixel_depth);
    float c_1h = z.x / a;
    float c_2h = (z.y - b * c_1h) / d;
    float c_3h = (z.z - c * c_1h - e * c_2h) / f;

    float c3 = c_3h / f;
    float c2 = (c_2h - e * c3) / d;
    float c1 = (c_1h - b * c2 - c * c3) / a;      

    float zf = pixel_depth;
    float z2 = (-c2 - sqrt(c2 * c2 - 4 * c3 * c1)) / (2 * c3);
    float z3 = (-c2 + sqrt(c2 * c2 - 4 * c3 * c1)) / (2 * c3);
            
    // can't believe glsl doesn't have swap smh
    if(z2 > z3) { float tmp = z2; z2 = z3; z3 = tmp; }
                     
    if(pixel_depth <= z2) {
      return 0.0;
    }
    else if (pixel_depth <= z3) {
      return (zf * z3 - new_b.x * (zf + z3) + new_b.y) / ((z3 - z2) * (zf - z2));
    }
    else {
      return 1.0 - (z2 * z3 - new_b.x * (z2 + z3) + new_b.y) / ((zf - z2) * (zf - z3));
    }
}