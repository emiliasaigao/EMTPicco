#version 310 es

layout(push_constant) uniform _unused_name_constant {
    highp float brightness_threshold;  
} bloom_effect;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInput in_color;
layout(set = 0, binding = 1) uniform highp sampler2D in_ssao;


layout(location = 0) in highp vec2 in_ndc_texcoord;
layout(location = 0) out highp vec4 out_color;
layout(location = 1) out highp vec4 out_bright_color;

highp float kernel[25] = float[25](
    1.0,  4.0,  7.0,  4.0, 1.0,
    4.0, 16.0, 26.0, 16.0, 4.0,
    7.0, 26.0, 41.0, 26.0, 7.0,
    4.0, 16.0, 26.0, 16.0, 4.0,
    1.0,  4.0,  7.0,  4.0, 1.0
);


void main() {

	ivec2 uv = ivec2(round(gl_FragCoord.x),round(gl_FragCoord.y));
    ivec2 right_top_point = textureSize(in_ssao, 0) - 1;
    highp float ssao_result = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            ssao_result += texelFetch(in_ssao, clamp(uv + ivec2(x,y), ivec2(1), right_top_point), 0).r * kernel[(x+2)*5+y+2] / 273.0;
        }
    }
    out_color = vec4(ssao_result) * subpassLoad(in_color).rgba;
    highp float brightness = dot(out_color.rgb, vec3(0.2126, 0.7152, 0.0722));
    out_bright_color = step(bloom_effect.brightness_threshold, brightness) * out_color / (max(out_color.r, max(out_color.g, out_color.b))+0.00001);
}