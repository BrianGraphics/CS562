#version 460

out vec4 FragColor;

float pi = 3.14159;
float pi2 = 2.0*pi;

in vec3 eyePos;

// lighting data
uniform vec3  lightPos;
uniform vec3  lightVal;
uniform vec3  lightAmb;

uniform float exposure;
uniform sampler2D skyTex;

uniform bool specularOn;
uniform float testblock[2*100];


vec3  BRDF_F(vec3 Ks, vec3 L, vec3 H);
float BRDF_D(float alpha, vec3 H, vec3 N);
float BRDF_G(float alpha, vec3 L, vec3 V, vec3 N);
float G1(float alpha, vec3 v, vec3 N);

uniform sampler2D g_buffer_world_pos;
uniform uint width, height;
uniform float AO_scale, AO_contrast;

vec4 BRDF(vec3 Pos, vec3 N, vec3 Kd, vec3 Ks, float alpha)
{
    // isSky
    if(alpha == 6969) return vec4(Kd, 1.0);

    // GGX alpha
    alpha = 1.0f / sqrt((alpha + 2.0f) / 2.0f);

    N = normalize(N);
    vec3 V = normalize(eyePos   - Pos);
    vec3 L = normalize(lightPos - Pos);  
    vec3 H = normalize(L + V);
    vec3 Ii = lightVal;
    vec3 Ia = lightAmb;
    

    // Ambient Occlusion
    vec3 Pi[9];
    vec2 Pi_uv[9];
    int num_points = 9;
    float influence = 5.0;

    float x_p = gl_FragCoord.x, y_p = gl_FragCoord.y;
    vec2 worldPos_uv = gl_FragCoord.xy / vec2(width, height);
    float depth = texture2D(g_buffer_world_pos, worldPos_uv).w;

    float phi = (30.0 *  float(int(x_p) ^ int(y_p))) + 10.0 * x_p * y_p;

    for(int i = 0; i < num_points; ++i) {
        Pi[i] = vec3(0.0);
        float a = (float(i) + 0.5) / float(num_points);
        float h = a * influence / depth;
        float theta = 2 * pi * a * (7.0 * float(num_points) / 9.0) + phi;

        Pi_uv[i] = worldPos_uv + h * vec2(cos(theta), sin(theta));
        Pi[i] = texture2D(g_buffer_world_pos, Pi_uv[i]).xyz;
    }

    float AO = 0.0;
    float AO_S = 0.0;
    float AO_threshhold = 0.001;
    float falloff = 0.1 * influence;
    for(int i = 0; i < num_points; ++i) {
        vec3 dir = normalize(Pi[i] - Pos);
        float di = texture2D(g_buffer_world_pos, Pi_uv[i]).w; // depth of selected points
        
        float H_factor = 0.0;
        if(influence > abs(di - depth)) H_factor = 1.0;
        AO_S += max(0.0, dot(N, dir) - AO_threshhold * di) * H_factor / max(falloff * falloff, dot(dir, dir)); 
    }

    AO_S *= pi2 * falloff / float(num_points);
    AO = max(0.0, pow(1.0 - AO_scale * AO_S, AO_contrast));


    // SRGB -> linear
    Kd  = pow(Kd,  vec3(2.2));
    Ks  = pow(Ks,  vec3(2.2));

    // diffuse part
    vec3 diffuse = Kd / pi;

    // specular part
    vec3  F_term = BRDF_F(Ks, L, H);
    float D_term = BRDF_D(alpha, H, N);
    float G_term = BRDF_G(alpha, L, V, N);
    float LdotN = max(dot(L, N), 0.0), VdotN = dot(V, N);
    vec3  specular =  F_term * G_term * D_term / (4 * LdotN * VdotN);
        
    if(!specularOn) specular = vec3(0.0);

    // diffuse + specular
    vec3 result =lightAmb * Kd + Ii * LdotN * ( diffuse + specular );

    // linear -> SRGB
    result = pow(result, vec3(1.0/2.2));
    
    return vec4(result, AO);
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