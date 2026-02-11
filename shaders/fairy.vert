#version 450

const vec2 vertices[4] = {
    vec2(-1, -1),
    vec2(1, -1),
    vec2(-1, 1),
    vec2(1, 1)
};

void main()
{
    vec2 vertex = vertices[gl_VertexIndex];
    gl_Position = vec4(vertex, 0.0, 1.0);
}
