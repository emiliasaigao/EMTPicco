#version 310 es

layout(location = 0) out highp vec2 out_texcoord;

void main()
{
    const vec3 fullscreen_triangle_positions[3] =
        vec3[3](vec3(3.0, 1.0, 0.5), vec3(-1.0, 1.0, 0.5), vec3(-1.0, -3.0, 0.5));
    gl_Position = vec4(fullscreen_triangle_positions[gl_VertexIndex], 1.0);
    out_texcoord = (fullscreen_triangle_positions[gl_VertexIndex].xy + 1.0) * 0.5;
}