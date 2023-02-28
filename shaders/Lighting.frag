#version 330

out vec4 FragColor;


uniform sampler2D g_buffer_world_pos;
uniform sampler2D g_buffer_world_norm;
uniform sampler2D g_buffer_diffuse_color;
uniform sampler2D g_buffer_specular_color;

uniform uint width, height;

// config
uniform int ID;
uniform int Toggle;
uniform vec3  lightAmb;
vec3 BRDF(vec3 Pos, vec3 N, vec3 Kd, vec3 Ks, float alpha);

void main()
{
    vec2 uv         = gl_FragCoord.xy / vec2(width, height);
    vec4 WorldPos_d = texture(g_buffer_world_pos,      uv);
    vec4 Normal_d   = texture(g_buffer_world_norm,     uv);
    vec4 Kd_d       = texture(g_buffer_diffuse_color,  uv);
    vec4 Ks_d       = texture(g_buffer_specular_color, uv);

    switch( ID )
    {
      case 1:
        FragColor.xyz = WorldPos_d.xyz / 10.0;
        return;
      case 2:
        if(Toggle == 0)
          FragColor = Normal_d;
        else if(Toggle == 1)
          FragColor = -Normal_d;
        else
          FragColor = abs(Normal_d);
        return;
      case 3:
        FragColor = Kd_d;
        return;
      case 4:
        FragColor = Ks_d;
        return;
    }
       
    FragColor.xyz = BRDF(WorldPos_d.xyz, Normal_d.xyz, Kd_d.xyz, Ks_d.xyz, Ks_d.w);            
}
