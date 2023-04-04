#version 460

out vec4 FragColor;

in vec3 normalVec;
in vec3 worldPos;

uniform uint width, height;

uniform sampler2D g_buffer_world_pos;
uniform sampler2D g_buffer_world_norm;
uniform sampler2D g_buffer_diffuse_color;
uniform sampler2D g_buffer_specular_color;

vec3 BRDF(vec3 Pos, vec3 N, vec3 Kd, vec3 Ks, float alpha);

void main()
{   
    // get position
    vec2 uv         = gl_FragCoord.xy / vec2(width, height);
    vec3 pos        = texture(g_buffer_world_pos,      uv).xyz;
    
    vec3 Normal_d   = texture(g_buffer_world_norm,     uv).xyz;
    vec3 Kd_d       = texture(g_buffer_diffuse_color,  uv).xyz;
    vec4 Ks_d       = texture(g_buffer_specular_color, uv);
      
    FragColor.xyz += BRDF(pos, Normal_d, Kd_d, Ks_d.xyz, Ks_d.w);         
}
