#version 430

layout(location = 0) flat in vec3 vColor;
layout(location = 1) noperspective in vec2 vOffset;
layout(location = 2) flat in float vRadius;
layout(location = 3) noperspective in float vNoiseSeed;

layout(location = 0) out vec4 outColor;

float hash(vec2 p)
{
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

float noise(vec2 uv)
{
    vec2 i = floor(uv);
    vec2 f = fract(uv);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

void main()
{
    float dist = length(vOffset);
    float r = max(vRadius, 0.0001);
    vec2 uv = vOffset / r;
    float n = noise(uv * 3.5 + vNoiseSeed);
    float mask = clamp(1.0 - dist / r, 0.0, 1.0);
    mask = pow(mask, 1.5);
    mask *= smoothstep(0.0, 0.4, n);
    if (mask <= 0.001)
        discard;
    float alpha = mask;
    // Simple submix: treat incoming color as CMY (inverted RGB) and convert back.
    vec3 rgb = vColor;
    vec3 cmy = 1.0 - clamp(rgb, 0.0, 1.0);
    cmy = 1.0 - (1.0 - cmy) * (1.0 - alpha); // lighten where alpha is strong
    vec3 mixed = 1.0 - cmy;

    vec3 col = mix(rgb, mixed, 0.4) * alpha;
    outColor = vec4(col, alpha);
}
