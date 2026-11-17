#version 430 core

// One triangle that covers the whole screen, generated from gl_VertexID (no vertex buffer):
// vertices (-1,-1), (3,-1), (-1,3).
void main()
{
    const vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
