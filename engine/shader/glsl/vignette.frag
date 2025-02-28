#version 310 es

layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInput in_color;

layout(push_constant) uniform _unused_name_constant {
    highp float cutoff;
    highp float exponent;
} vignette_effect;

layout(location = 0) in highp vec2 in_texcoord;
layout(location = 0) out highp vec4 out_color;

void main()
{
    highp vec4 color = subpassLoad(in_color).rgba;

    highp float len = length(in_texcoord - 0.5f);
    highp float ratio = pow(vignette_effect.cutoff / len, vignette_effect.exponent);

    out_color = min(1.0, ratio) * color;
}


