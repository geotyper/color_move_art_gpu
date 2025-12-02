#version 430

layout(binding = 0) uniform sampler samplerLinear;
layout(binding = 1) uniform texture2D canvasTex;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 c = texture(sampler2D(canvasTex, samplerLinear), vUV);
    float vignette = smoothstep(1.05, 0.7, length(vUV - 0.5));
    outColor = vec4(c.rgb * vignette + vec3(0.02), 1.0);
}
