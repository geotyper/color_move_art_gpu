#version 430

layout(std140, binding = 0) uniform ViewParams {
    vec4 canvas_radius; // x,y: canvas size, z: radius
};

layout(location = 0) in vec4 aPosColor; // xy: position, zw: red/green (blue baked in frag)

noperspective layout(location = 0) out vec3 vColor;
noperspective layout(location = 1) out vec2 vOffset;
noperspective layout(location = 2) out float vRadius;

// Nine vertices forming a 3x3 grid around the center so the fragment shader can discard corners.
const vec2 corners[9] = vec2[](
    vec2(-1.0, -1.0), vec2(0.0, -1.0), vec2(1.0, -1.0),
    vec2(-1.0,  0.0), vec2(0.0,  0.0), vec2(1.0,  0.0),
    vec2(-1.0,  1.0), vec2(0.0,  1.0), vec2(1.0,  1.0)
);

const uint idxs[24] = uint[](
    0u, 1u, 4u,
    0u, 4u, 3u,
    1u, 2u, 5u,
    1u, 5u, 4u,
    3u, 4u, 7u,
    3u, 7u, 6u,
    4u, 5u, 8u,
    4u, 8u, 7u
);

void main()
{
    vec4 p = aPosColor;
    float radius = max(2.0, canvas_radius.z);
    uint cornerIdx = idxs[gl_VertexIndex];
    vec2 corner = corners[cornerIdx] * radius;
    vec2 pos = p.xy + corner;

    vec2 canvasSize = canvas_radius.xy;
    vec2 ndc;
    ndc.x = (pos.x / canvasSize.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (pos.y / canvasSize.y) * 2.0;

    gl_Position = vec4(ndc, 0.0, 1.0);

    vColor = vec3(p.z, p.w, 0.5);
    vOffset = corner;
    vRadius = radius;
}
