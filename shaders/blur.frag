#version 430

layout(binding = 0) uniform sampler samplerLinear;
layout(binding = 1) uniform texture2D srcTex;

layout(std140, binding = 2) uniform BlurParams {
    vec4 dir_decay; // xy: direction (in texels), z: decay factor
};

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 dir = dir_decay.xy;
    float decay = dir_decay.z;
    vec4 c = texture(sampler2D(srcTex, samplerLinear), vUV) * 0.24;
    c += texture(sampler2D(srcTex, samplerLinear), vUV + dir * 1.0) * 0.20;
    c += texture(sampler2D(srcTex, samplerLinear), vUV - dir * 1.0) * 0.20;
    c += texture(sampler2D(srcTex, samplerLinear), vUV + dir * 2.0) * 0.14;
    c += texture(sampler2D(srcTex, samplerLinear), vUV - dir * 2.0) * 0.14;
    c += texture(sampler2D(srcTex, samplerLinear), vUV + dir * 3.0) * 0.08;
    c += texture(sampler2D(srcTex, samplerLinear), vUV - dir * 3.0) * 0.08;
    c *= decay;
    outColor = c;
}
