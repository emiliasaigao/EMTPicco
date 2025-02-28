#version 310 es

#extension GL_GOOGLE_include_directive : enable

#include "constants.h"
#include "gbuffer.h"

layout(std140, set = 0, binding = 0) readonly buffer _unused_name_perframe {
    highp mat4 proj_matrix;
    highp mat4 proj_inv_matrix;
    highp mat4 view_matrix;

    highp float hbao_intensity;
    highp float hbao_radius;
    highp float hbao_max_radius_pixels;
    highp float hbao_angle_bias;
    
    highp float hbao_radius_pixel;
    highp float hbao_neg_inv_radius2;
    highp float windowWidth;
    highp float windowHeight;
};

layout(input_attachment_index = 0, set = 0, binding = 1) uniform highp subpassInput in_normal;

layout(set = 0, binding = 2) uniform highp sampler2D in_depth;
layout(set = 0, binding = 3) uniform highp sampler2D ssao_noise;

layout(location = 0) in highp vec2 in_texcoord;
layout(location = 0) out highp vec4 out_color;

// 距离衰减
highp float FallOff(highp float distanceSquare)
{
	return distanceSquare * hbao_neg_inv_radius2 + 1.0;
}

highp float saturate(highp float x) {
    return clamp(x, 0.0, 1.0);
}

highp float ComputeAO(highp vec3 vpos, 
                      highp vec3 stepVpos, 
                      highp vec3 normal) {
    highp vec3 v = stepVpos - vpos;
    highp float dist = length(v);
    highp float VoV = dist * dist;
    highp float NoV = dot(normal, v) / dist;

    return saturate(NoV - hbao_angle_bias) * saturate(FallOff(VoV));
}


highp vec3 GetViewPos(highp vec2 uv) {
    highp float depth = texture(in_depth, uv).r;
    highp vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth, 1.0);
    highp vec4 viewSpace = proj_inv_matrix * clipSpace;
    highp vec3 viewPos = viewSpace.xyz / viewSpace.w;
    return viewPos;
}

highp float Random(highp vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453123);
}

void main() {
    highp vec4 windowSize = vec4(windowWidth, windowHeight, 1.0 / windowWidth, 1.0/ windowHeight);

    highp vec3 vpos = GetViewPos(in_texcoord);
    highp vec3 raw_normal = subpassLoad(in_normal).rgb;
    if (raw_normal == vec3(0)) {
        out_color = vec4(1.0);
        return;
    }

    highp vec3 normal = raw_normal * 2.0 - 1.0;
    normal = mat3(view_matrix) * normal;
    normal = normalize(normal);

    highp vec2 noise = vec2(Random(in_texcoord.yx), Random(in_texcoord.xy));

    // 计算步近值
    highp float stride = min(hbao_radius_pixel / abs(vpos.z), hbao_max_radius_pixels) / (float(HBAO_STEP_COUNT) + 1.0);
    // stride至少大于一个像素
    if (stride < 1.0) {
        out_color = vec4(1.0);
        return;
    }

    highp float stepRadian = (PI * 2.0) / float(HBAO_DIRECTION_COUNT);

    highp float ao = 0.0;

    for (int d = 0; d < HBAO_DIRECTION_COUNT; d++) {
        // 计算起始随机步近方向
        highp float radian = stepRadian * (float(d) + noise.x);
        highp float sinr = sin(radian);
        highp float cosr = cos(radian);
        highp vec2 direction = vec2(cosr, sinr);

        // 计算起始随机步近长度
        highp float rayPixels = fract(noise.y) * stride + 1.0;

        // 进行光线步近

        for (int s = 0; s < HBAO_STEP_COUNT; s++) {
            highp vec2 uv2 = round(rayPixels * direction) * windowSize.zw + in_texcoord;
            highp vec3 vpos2 = GetViewPos(uv2);
            ao += ComputeAO(vpos, vpos2, normal);
            rayPixels += stride;
        }
    }

    // 提高对比度
    highp float factor = 1.0 / float(HBAO_STEP_COUNT * HBAO_DIRECTION_COUNT);
    ao = pow(ao * factor, 0.6);

    out_color = vec4(mix(vec3(1.0),vec3(1.0 - ao), hbao_intensity), 1.0);
}
