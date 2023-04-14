#version 460

out vec4 FragColor;

uniform float width, height;
uniform sampler2D blurredAO;

void main()
{
    vec2 uv  = gl_FragCoord.xy / vec2(width, height);
    vec4 result = texture2D(blurredAO, uv);
    FragColor.xyz = result.xyz * result.w;       
}
