#version 310 es

#extension GL_GOOGLE_include_directive : enable

#include "constants.h"


layout(set = 2, binding = 0) uniform _unused_name_permaterial
{
    int  _area; // 0 身体， 1 头发， 2 脸

    highp vec3  _front_face_tint_color;
    lowp  float padding_front_face;
    highp vec3  _back_face_tint_color;
    lowp  float padding_back_face;
    
    highp float _alpha;
    highp float _alpha_clip;
    lowp  vec2 padding_alpha;

    highp float _indirect_light_usage;
    highp float _indirect_light_mix_base_color;
    highp float _indirect_light_occlusion_usage;
    highp float _main_light_color_usage;
    highp float _shadow_threshold_center;
    highp float _shadow_threshold_softness;
    highp float _shadow_ramp_offset;
    lowp  float padding_lighting;

    highp float _face_shadow_offset;
    highp float _face_shadow_transition_softness;
    lowp  vec2 padding_face_shadow;

    highp float _specular_exponent;
    highp float _specular_Ks_non_metal;
    highp float _specular_Ks_metal;
    highp float _specular_brightness;


    highp float _rim_light_width;
    highp float _rim_light_threshold;
    highp float _rim_light_fadeout;
    highp float _rim_light_brightness;
    
    highp vec3  _rim_light_tint_color;
    lowp  float padding_rim_light_tint;

    highp float _rim_light_mix_albedo;
    
    highp vec3  _emission_tint_color;
    lowp  float padding_emission_tint;

    highp float _emission_mix_base_color;
    highp float _emission_intensity;
    lowp  vec2 padding_emission;

};

layout(set = 2, binding = 1) uniform highp sampler2D base_color_texture_sampler;
layout(set = 2, binding = 2) uniform highp sampler2D light_map_texture_sampler;
layout(set = 2, binding = 3) uniform highp sampler2D ramp_warm_sampler;
layout(set = 2, binding = 4) uniform highp sampler2D ramp_cool_sampler;
layout(set = 2, binding = 5) uniform highp sampler2D face_map_sampler;

layout(push_constant) uniform _unused_name_constant {
    highp float  outline_width;
    highp float  outline_z_offset;
    highp float  outline_gamma;
} push_constant;

// read in fragnormal (from vertex shader)
layout(location = 0) in highp vec4 in_clip_position;
layout(location = 1) in highp vec2 in_texcoord;

layout(location = 0) out highp vec4 out_color;

highp float saturate(highp float x) {
    return clamp(x, 0.0, 1.0);
}

highp vec3 saturate(highp vec3 x) {
    return clamp(x, vec3(0.0), vec3(1.0));
}


void main()
{
    int rampRowCount = 1;
    int rampRowIndex = 0;

    if (_area == 0) {
        int rowIndex = int(round(texture(light_map_texture_sampler, in_texcoord).a * 7.0 * 1.053));
        rampRowCount = 8;
        rampRowIndex = rowIndex < 4 ? rowIndex : (11 - rowIndex);
    }
    else if(_area == 1) {
        rampRowCount = 1;
        rampRowIndex = 0;
    }
    else {
        rampRowCount = 8;
        rampRowIndex = 0;
    }
    highp float rampUVx = 0.5;
    highp float rampUVy = (2.0 * float(rampRowIndex) + 1.0) / (float(rampRowCount) * 2.0);
    highp vec2 rampUV = vec2(rampUVx, rampUVy);
    highp vec3 rampWarm = texture(ramp_warm_sampler, rampUV).rgb;
    highp vec3 rampCool = texture(ramp_cool_sampler, rampUV).rgb;

    highp vec3 rampColor = mix(rampCool, rampWarm, 0.5);
    highp vec3 color = pow(saturate(rampColor), vec3(push_constant.outline_gamma));
    out_color = vec4(color, 1.0);
}
