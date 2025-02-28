#version 310 es

layout(set = 0, binding = 0) uniform sampler2D in_color;

layout(location = 0) in highp vec2 in_texcoord;
layout(location = 0) out highp vec4 out_color;

void main()
{
    highp vec4 color = texture(in_color, in_texcoord).rgba;
    out_color = color;
}


