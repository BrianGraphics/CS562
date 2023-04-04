#version 460

out vec4 FragColor;

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

uniform float exposure;
uniform sampler2D irrMap;
uniform sampler2D skyTex;
uniform float skyWidth, skyHeight;

uniform HammersleyBlock {
 float pairs;
 uint id;
 float hammersley[2*100]; 
};

uniform bool specularOn;
uniform float testblock[2*100];


vec3  BRDF_F(vec3 Ks, vec3 L, vec3 H);
float BRDF_D(float alpha, vec3 H, vec3 N);
float BRDF_G(float alpha, vec3 L, vec3 V, vec3 N);
float G1(float alpha, vec3 v, vec3 N);

// Spherical  Harmonics
uniform sampler2D SHCoeff;


vec3 BRDF(vec3 Pos, vec3 N, vec3 Kd, vec3 Ks, float alpha)
{
    // isSky
    if(alpha == 6969) return Kd;

    // GGX alpha
    alpha = 1.0f / sqrt((alpha + 2.0f) / 2.0f);

    N = normalize(N);
    vec3 V = normalize(eyePos   - Pos);    
    //vec3 H = normalize(L + V);  
    //vec3 R = -1.0 * ( 2 * dot(V,N) * N - V);
    //vec3 L = normalize(-R);   
    
    // iradiance
    vec2 irr_uv = vec2(atan(N.y,N.x)/(2*pi), -acos(N.z)/pi);
    vec3 irr = textureLod(SHCoeff, irr_uv, 2).xyz;
    


    // SRGB -> linear
    Kd  = pow(Kd,  vec3(2.2));
    Ks  = pow(Ks,  vec3(2.2));
    irr = pow(irr, vec3(2.2));

    // diffuse part
    vec3 diffuse = Kd / pi * irr;

    // specular part
    vec3 specular;
    for(int i = 0; i < max(pairs, 1); ++i) {
        float rnd1 = hammersley[i * 2];
        float rnd2 = hammersley[i * 2 + 1];
        
        vec2 uv = vec2(rnd1, atan(alpha * sqrt(rnd2) / sqrt(1 - rnd2)) / pi);
        vec3 L = vec3(cos(2*pi * (0.5 - uv.x)) * sin(pi * uv.y), sin(2*pi * (0.5 - uv.x)), cos(pi * uv.y));
        vec3 R = 2 * dot(V,N) * N - V;

        vec3 rA = normalize(vec3(-R.y, -R.x, 0.0));
        vec3 rB = normalize(cross(R, rA));

        L = normalize(L.x*rA + L.y*rB + L.z*R);
        vec3 H = normalize(L + V);  

        // sky dome
        //vec2 sky_uv = vec2(0.5 - atan(L.y, L.x) / (2 * pi), acos(L.z) / pi);         
        R = -R;
        vec2 sky_uv = vec2(-atan(R.y,R.x)/(2*pi), acos(R.z)/pi);
        float level = 0.5 * log2(skyWidth * skyHeight /pairs) - 0.5 * log2(BRDF_D(alpha, H, N) / 4.0);

        vec3 Ii;
        if(pairs == 0)
            Ii = texture2D(skyTex, sky_uv).xyz;
        else
            Ii =  textureLod(skyTex, sky_uv, max(level, 0.0)).xyz;   

        // SRGB -> linear
        Ii  = pow(Ii,  vec3(2.2));


        vec3  F_term = BRDF_F(Ks, L, H);
        //float D_term = BRDF_D(alpha, H, N);
        float G_term = BRDF_G(alpha, L, V, N);
        float LdotN = max(dot(L, N), 0.0), VdotN = dot(V, N);
        vec3  BRDF_part =  F_term * G_term / (4 * LdotN * VdotN);
        //vec3  BRDF_part =  (F_term * G_term * D_term / (4 * LdotN * VdotN));

        specular += Ii * LdotN * BRDF_part;
    }

    // avg
    specular = specular / max(pairs, 1.0);

    if(!specularOn) specular = vec3(0.0);

    // diffuse + specular
    vec3 result = diffuse + specular;

    // linear -> SRGB
    result = exposure * result / (exposure * result + vec3(1.0));
    result = pow(result, vec3(1.0/2.2));
    
    return result;
}

vec3 BRDF_F(vec3 Ks, vec3 L, vec3 H) 
{
    return Ks + (vec3(1.0) - Ks) * pow((1.0 - dot(L,H)), 5);
}

float BRDF_D(float alpha, vec3 H, vec3 N) 
{
    float HdotN = max(dot(H, N), 0.0);
    return (alpha * alpha) / pi / pow(HdotN * HdotN * (alpha * alpha - 1.0) + 1.0, 2);
}

float BRDF_G(float alpha, vec3 L, vec3 V, vec3 N) 
{
    return G1(alpha, L, N) * G1(alpha, V, N);
}

float G1(float alpha, vec3 v, vec3 N) {
    float VdotN = dot(v, N);
    float tan = sqrt(1.0 - VdotN * VdotN) / VdotN;

    if(tan <= 0)
        return 1.0;
    else
        return 2.0 / (1.0 + sqrt(1.0 + alpha * alpha * tan * tan));
}