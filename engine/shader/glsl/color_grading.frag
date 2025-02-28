#version 310 es

#extension GL_GOOGLE_include_directive : enable

#include "constants.h"

layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInput in_color;

layout(set = 0, binding = 1) uniform sampler2D color_grading_lut_texture_sampler;

layout(push_constant) uniform _unused_name_constant {
    highp float useColorGrading;
    highp float brightness;  // 亮度调整
    highp float contrast;    // 对比度调整
    highp float saturation;  // 饱和度调整
    highp float temperature; // 色温
} color_grading_effect;

layout(location = 0) out highp vec4 out_color;

// 白平衡调整矩阵
highp mat3 temperatureMatrix(highp float temp) {
    temp = clamp(temp, -1.0, 1.0);
    highp float t1 = temp * 0.2;
    return mat3(
        vec3(1.0 + t1, -t1, -t1),
        vec3(-t1, 1.0 + t1, -t1),
        vec3(-t1, -t1, 1.0 + t1)
    );
}

void main()
{
    highp ivec2 lut_tex_size = textureSize(color_grading_lut_texture_sampler, 0);
    highp float _COLORS      = float(lut_tex_size.y);

    highp vec4 color       = subpassLoad(in_color).rgba;

    // colorAjustment
    // 1. 亮度调整：通过增加或减少颜色的亮度
    color.rgb += color_grading_effect.brightness;
    color.rgb = clamp(color.rgb, 0.0, 1.0);

    // 2. 对比度调整：改变颜色的对比度
    color.rgb = (color.rgb - 0.5) * color_grading_effect.contrast + 0.5;

    // 3. 饱和度调整：通过将颜色转化为灰度再重新插值来调整饱和度
    highp vec3 luminance = vec3(dot(color.rgb, vec3(0.2126, 0.7152, 0.0722)));
    color.rgb = luminance + (color.rgb - luminance) * color_grading_effect.saturation; // 线性放大
    color.rgb = clamp(color.rgb, 0.0, 1.0); // 防止溢出
    
    color.rgb = temperatureMatrix(color_grading_effect.temperature) * color.rgb;

    highp float b = color.b * _COLORS;
    highp float b_floor = floor(b);
    highp float b_ceil = ceil(b);
    highp vec4 color_floor  = texture(color_grading_lut_texture_sampler, vec2((b_floor + color.r)/_COLORS, color.g));
    highp vec4 color_ceil   = texture(color_grading_lut_texture_sampler, vec2((b_ceil + color.r)/_COLORS, color.g));

    out_color = mix(color, mix(color_floor, color_ceil, b - b_ceil), color_grading_effect.useColorGrading);
}
