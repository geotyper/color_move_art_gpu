#version 430

layout(location = 0) flat in vec3 vColor;
layout(location = 1) noperspective in vec2 vOffset;
layout(location = 2) flat in float vRadius;

layout(location = 0) out vec4 outColor;

void main()
{
    float dist = length(vOffset);
    float t = clamp(1.0 - dist / max(vRadius, 0.0001), 0.0, 1.0);
    if (t <= 0.0)
        discard;
    float alpha = t * t;
    // Simple submix: treat incoming color as CMY (inverted RGB) and convert back.
    vec3 rgb = vColor;
    vec3 cmy = 1.0 - clamp(rgb, 0.0, 1.0);
    cmy = 1.0 - (1.0 - cmy) * (1.0 - alpha); // lighten where alpha is strong
    vec3 mixed = 1.0 - cmy;

    vec3 col = mix(rgb, mixed, 0.4) * alpha;
    outColor = vec4(col, alpha);
}
