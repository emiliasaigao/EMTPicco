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

layout(set = 0, binding = 0) readonly buffer _unused_name_perframe
{
    highp mat4   proj_view_matrix;
    highp mat4   proj_matrix;
    highp mat4   view_matrix;
    highp vec3   camera_position;
    lowp float  _padding_camera_position;
    
    DirectionalLight scene_directional_light;
};

layout(set = 0, binding = 3) uniform samplerCube irradiance_sampler;
layout(set = 0, binding = 4) uniform highp sampler2D depth_sampler;


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

// read in fragnormal (from vertex shader)
layout(location = 0) in highp vec3 in_world_position;
layout(location = 1) in highp vec4 in_clip_position;
layout(location = 2) in highp vec3 in_normal;
layout(location = 3) in highp vec3 in_view_dir;
layout(location = 4) in highp vec2 in_texcoord;

layout(location = 0) out highp vec4 out_color;
layout(location = 1) out highp vec4 out_normal;

highp float saturate(highp float x) {
    return clamp(x, 0.0, 1.0);
}

highp float LinearEyeDepth(highp float depth) {
    return proj_matrix[3][2] / (depth + proj_matrix[2][2]);
}

void main()
{
    highp float alpha = _alpha;
    if (alpha < _alpha_clip) {
        discard;
    }

    highp vec2 uv         = in_texcoord;
    highp vec3 normalWS   = normalize(in_normal);
    highp vec3 viewDirWS  = normalize(in_view_dir);
    highp vec3 lightDirWS = normalize(scene_directional_light.direction);
    highp vec3 positionCS = in_clip_position.xyz / in_clip_position.w;
    positionCS.xy         = positionCS.xy * 0.5 + 0.5;

    out_normal.rgb = normalWS * 0.5 + 0.5;

    highp vec4  areaMap = texture(base_color_texture_sampler, uv);
    highp vec3 baseColor = areaMap.rgb;
    baseColor *= mix(_back_face_tint_color, _front_face_tint_color, float(gl_FrontFacing));

    highp vec4 lightMap   = vec4(0.0);
    if (_area != 2) {
        lightMap = texture(light_map_texture_sampler, uv);
    }
    highp vec4 faceMap    = vec4(0.0);
    if (_area == 2) {
        faceMap = texture(face_map_sampler, uv);
    }


    highp vec3 origin_samplecube_N = vec3(normalWS.x, normalWS.z, normalWS.y);
    highp vec3 indirectLightColor = texture(irradiance_sampler, origin_samplecube_N).rgb * _indirect_light_usage * 0.5;
    if (_area == 2) {
        indirectLightColor *= mix(1., mix(faceMap.g, 1., step(faceMap.r,0.2)), _indirect_light_occlusion_usage);
    }else {
        indirectLightColor *= mix(1.0, lightMap.r, _indirect_light_occlusion_usage);
    }
    indirectLightColor *= mix(vec3(1.0), baseColor, _indirect_light_mix_base_color);


    highp vec3 mainLightColor = scene_directional_light.color;
    highp float mainLightShadow = 1.0;
    int rampRowCount = 1;
    int rampRowIndex = 0;
    if (_area != 2) {
        highp float NdotL = dot(normalWS,lightDirWS);
        highp float remappedNdotL = NdotL * 0.5 + 0.5;
        
        mainLightShadow = smoothstep(1.0 - lightMap.g + _shadow_threshold_center - _shadow_threshold_softness,
                                     1.0 - lightMap.g + _shadow_threshold_center + _shadow_threshold_softness,
                                     remappedNdotL);
        int rowIndex = int(round(lightMap.a * 7. * 1.053));
        if (_area == 0) {
            rampRowCount = 8;
            rampRowIndex = rowIndex < 4 ? rowIndex : (11 - rowIndex);
        }else {
            rampRowCount = 1;
            rampRowIndex = 0;
        }
    }else {
        highp vec3 headForward = vec3(1.0, 0.0, 0.0);
        highp vec3 headRight = vec3(0.0, -1.0, 0.0);
        highp vec3 headUp = vec3(0.0, 0.0, 1.0);

        highp vec3 fixedLightDirWS = normalize(lightDirWS - dot(lightDirWS, headUp) * headUp); // 计算光线方向在头坐标系的水平面上的投影
        // 通过光投影向量点乘头右方向判断光照在脸左还是脸右，如果是正数说明照在脸右，那么直接采样，否则镜像翻转采样
        highp vec2 sdfUV = vec2(sign(dot(fixedLightDirWS, headRight)), 1.) * uv * vec2(-1., 1.);
        highp float sdfValue = texture(face_map_sampler, sdfUV).a;
        sdfValue += _face_shadow_offset; // 加一个偏移值，以防光照在正背面时sdf采样到边界值1，通过step使得脸全亮
        
        // dot(headForward, fixedLightDirWS)越大说明光和前方越重合，越不应该有阴影，所以用1-，阈值会很小
        highp float sdfThreshold = 1. - (dot(headForward, fixedLightDirWS) * 0.5 + 0.5);
        
        highp float sdf = smoothstep(sdfThreshold - _face_shadow_transition_softness, sdfThreshold + _face_shadow_transition_softness, sdfValue);
        mainLightShadow = mix(faceMap.g, sdf, step(faceMap.r, 0.5));

        rampRowCount = 8;
        rampRowIndex = 0;
    }
    highp float rampUVx = mainLightShadow * (1.0 - _shadow_ramp_offset) + _shadow_ramp_offset;
    highp float rampUVy = (2.0 * float(rampRowIndex) + 1.0) / (float(rampRowCount) * 2.0);
    highp vec2 rampUV = vec2(rampUVx, rampUVy);
    highp vec3 rampWarm = texture(ramp_warm_sampler, rampUV).rgb;
    highp vec3 rampCool = texture(ramp_cool_sampler, rampUV).rgb;

    highp float isDay = lightDirWS.z * 0.5 + 0.5;
    highp vec3 rampColor = mix(rampCool, rampWarm, isDay);
    mainLightColor *= rampColor * baseColor;


    highp vec3 specularLightColor = vec3(0.0);
    if (_area != 2) {
        highp vec3 H = normalize(lightDirWS + viewDirWS);
        highp float NoH = saturate(dot(normalWS,H));
        highp float blinnPhong = pow(NoH, _specular_exponent);

        // LightMap里b通道是金属高光阈值，说明了布林冯高光值大于多少才会产生高光
        // 非金属高光部分，将布林冯反向作为阶梯，越产生高光的地方阶梯越矮，阈值再小也能通过
        // 用1.04减是防止布林冯为1时阈值为0的（非高光）区域也反光
        highp float nonMetalSpecular = step(1.04 - blinnPhong, lightMap.b) * _specular_Ks_non_metal;

        highp float metalSpecular = blinnPhong * _specular_Ks_metal * lightMap.b;

        // 区分金属和非金属，lightMap的a通道里金属被标记为大约0.686的值
        highp float metallic = 0.;
        if (_area == 0)  {
            metallic = saturate((abs(lightMap.a - 0.686) - 0.1) / (0.0 - 0.1)); // 在0-0.1的误差范围内则将其映射到0，1
        }
        specularLightColor = mix(vec3(nonMetalSpecular), metalSpecular * baseColor, metallic);
        specularLightColor *= mainLightColor;
        specularLightColor *= _specular_brightness;
    }


    highp float fakeOutlineEffect = 0.;
    highp vec3 fakeOutlineColor = vec3(0.0);
    if (_area == 2) {
        // 鼻线部分是1，其他地方都是0
        highp float fakeOutline = faceMap.b;
        // 视角与头前向量越贴近，显示越清晰
        fakeOutlineEffect = smoothstep(0.0, 0.25, saturate(pow(dot(vec3(1.0, 0.0, 0.0), viewDirWS), 10.0)) * fakeOutline);

        highp vec2 outlineUV = vec2(0.0, 0.0625);
        rampWarm = texture(ramp_warm_sampler, outlineUV).rgb;
        rampCool = texture(ramp_cool_sampler, outlineUV).rgb;
        rampColor = mix(rampCool, rampWarm, 0.5);
        fakeOutlineColor = pow(rampColor, vec3(64.0));
    }


    highp float linearEyeDepth = LinearEyeDepth(positionCS.z);  // 计算当前片元的观察空间深度
    highp vec3 normalVS = mat3(view_matrix) * normalWS;                  // 计算当前片元的观察空间法线方向
    highp vec2 uvOffset = vec2(sign(normalVS.x),0.) * _rim_light_width / (1.0 + linearEyeDepth) / 100.0; // 根据法线向左还是向右生成微小的偏移，除以1 + linearEyeDepth实现近大远小的效果
    highp vec2 sampleUV = positionCS.xy + uvOffset;  // 计算偏移后的屏幕空间坐标
    sampleUV = clamp(sampleUV, vec2(0.0), vec2(0.99));
    highp float offsetSceneDepth = texture(depth_sampler, sampleUV).r;                        // 查询偏移点处的z值
    highp float offsetLinearEyeDepth = LinearEyeDepth(offsetSceneDepth);                    // 计算偏移点处观察空间深度
    highp float rimLight = saturate(offsetLinearEyeDepth - (linearEyeDepth + _rim_light_threshold)) / _rim_light_fadeout; // 根据观察空间深度差计算边缘光的亮度
    highp vec3 rimLightColor = rimLight * scene_directional_light.color;
    rimLightColor *= _rim_light_tint_color;
    rimLightColor *= _rim_light_brightness;


    highp vec3 emissionColor = vec3(0.0);
    if (_area != 1) {
        emissionColor = vec3(areaMap.a);
        emissionColor *= mix(vec3(1.0), baseColor, _emission_mix_base_color);
        emissionColor *= _emission_tint_color;
        emissionColor *= _emission_intensity;
    }
    
    highp vec3 headForward = vec3(1.0, 0.0, 0.0);
    alpha = mix(1.0, alpha, saturate(dot(headForward, viewDirWS)));

    highp vec3 albedo = vec3(0.0);
    albedo += indirectLightColor;
    albedo += mainLightColor;
    albedo += specularLightColor;
    albedo += rimLightColor * mix(vec3(1.0), albedo, _rim_light_mix_albedo);
    albedo += emissionColor;
    albedo = mix(albedo, fakeOutlineColor, fakeOutlineEffect);

    out_color = vec4(albedo, alpha);

}
