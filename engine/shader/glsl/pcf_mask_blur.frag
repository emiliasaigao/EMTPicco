#version 310 es

layout(set = 0, binding = 0) uniform highp sampler2D in_pcf_mask;

layout(location = 0) in highp vec2 in_ndc_texcoord;
layout(location = 0) out highp float out_color;

highp float kernel[25] = float[25](
    1.0,  4.0,  7.0,  4.0, 1.0,
    4.0, 16.0, 26.0, 16.0, 4.0,
    7.0, 26.0, 41.0, 26.0, 7.0,
    4.0, 16.0, 26.0, 16.0, 4.0,
    1.0,  4.0,  7.0,  4.0, 1.0
);


void main() {

	ivec2 uv = ivec2(round(gl_FragCoord.x),round(gl_FragCoord.y));
    ivec2 right_top_point = textureSize(in_pcf_mask, 0) - 1;
    highp float pcf_mask_result = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            pcf_mask_result += texelFetch(in_pcf_mask, clamp(uv + ivec2(x,y), ivec2(1), right_top_point), 0).r * kernel[(x+2)*5+y+2] / 273.0;
        }
    }
    out_color = pcf_mask_result;
}