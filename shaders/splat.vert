#version 430

layout(std140, binding = 0) uniform ViewParams {
    vec4 canvas_radius; // x,y: canvas size, z: radius
};

layout(location = 0) in vec4 aPosColor; // xy: position, zw: red/green (blue baked in frag)

noperspective layout(location = 0) out vec3 vColor;
noperspective layout(location = 1) out vec2 vOffset;
noperspective layout(location = 2) out float vRadius;
noperspective layout(location = 3) out float vNoiseSeed;

// Five vertices forming a quad with center for distortion.
const vec2 corners[5] = vec2[](
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(-1.0, 1.0),
    vec2(0.0, 0.0)
);

const uint idxs[18] = uint[](
    0u, 1u, 4u,
    1u, 2u, 4u,
    2u, 3u, 4u,
    3u, 0u, 4u,
    0u, 1u, 2u,
    0u, 2u, 3u
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
    vNoiseSeed = float(gl_InstanceIndex) * 0.731 + float(gl_VertexIndex) * 0.123;
}
