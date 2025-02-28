#version 310 es

layout(location = 0) out highp float out_color;
void main()
{
    out_color = gl_FragCoord.z;
}