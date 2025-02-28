#version 310 es

#extension GL_GOOGLE_include_directive : enable

#include "constants.h"
struct DirectionalLight
{
    highp vec3 direction;
    lowp float _padding_direction;
    highp vec3 color;
    lowp float _padding_color;
};

struct PointLight
{
    highp vec3  position;
    highp float radius;
    highp vec3  intensity;
    lowp float  _padding_intensity;
};

layout(push_constant) uniform _unused_name_constant {
    highp mat4 inverse_proj_view_matrix;
    highp mat4 directional_light_proj_view;
} pushConstants;

layout(set = 0, binding = 0) uniform highp sampler2D in_depth;
layout(set = 0, binding = 1) uniform highp sampler2D directional_light_shadow;

layout(location = 0) in highp vec2 in_texcoord;
layout(location = 0) out highp float out_color;

highp vec2 random_offset(highp vec2 seed, highp float index) {
    highp float x = fract(sin(dot(seed + vec2(index * 13.0, index * 37.0), vec2(12.9898, 78.233))) * 43758.5453);
    highp float y = fract(sin(dot(seed + vec2(index * 17.0, index * 43.0), vec2(45.164, 97.345))) * 96548.3875);
    return vec2(x, y);
}

void main()
{
    highp mat4 inverse_proj_view_matrix = pushConstants.inverse_proj_view_matrix;
    highp mat4 directional_light_proj_view = pushConstants.directional_light_proj_view;

    highp vec2 seed = gl_FragCoord.xy * 0.01; // 让不同的像素有不同的随机种子
    highp vec2 texel_size = 1.0 / vec2(textureSize(in_depth, 0)); // 获取 in_color 的单个 texel 尺寸
    highp vec2 base_uv = in_texcoord - mod(in_texcoord, 4.0 * texel_size);   // 找到 4x4 块的左上角 UV

    int in_shadow_count = 0;
    for (int i = 0; i < 6; i++) {
        highp vec2 rand_offset = random_offset(seed, float(i)) * (3.0 * texel_size); // 让随机数落在 4x4 范围内
        highp vec2 uv = base_uv + rand_offset;
        highp float depth = texture(in_depth, uv).r;
        highp vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth, 1.0);
        highp vec4 worldSpace = inverse_proj_view_matrix * clipSpace;
        highp vec3 worldPos = worldSpace.xyz / worldSpace.w;

        highp vec4 lightClipSpace = directional_light_proj_view * vec4(worldPos, 1.0);
        highp vec3 lightNDCPos  = lightClipSpace.xyz / lightClipSpace.w;

        highp vec2 shadowMapUV = lightNDCPos.xy * 0.5 + 0.5;
        highp float closest_depth = texture(directional_light_shadow, shadowMapUV).r - 0.0005;
        highp float current_depth = lightNDCPos.z;

        in_shadow_count += (closest_depth >= current_depth) ? 0 : 1;
    }
    highp float factor = float(in_shadow_count) / 6.0;
    out_color = 1.0f-factor;
}


