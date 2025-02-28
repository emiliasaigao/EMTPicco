#version 310 es

#extension GL_GOOGLE_include_directive : enable

#include "constants.h"
#include "gbuffer.h"

layout(std140, set = 0, binding = 0) readonly buffer _unused_name_perframe {
    highp mat4 projection;
    highp mat4 view;
    highp vec4 samples[64];   // 自动从Vector4[3]转换 
	highp float windowWidth;
    highp float windowHeight;
    highp float SSAOEffect;
    highp float SSAORadius;
	highp float SSAOBias;
    highp int SSAOKernelSize;
    lowp float _padding[2];
};

layout(input_attachment_index = 0, set = 0, binding = 1) uniform highp subpassInput in_normal;

layout(set = 0, binding = 2) uniform sampler2D in_depth;
layout(set = 0, binding = 3) uniform sampler2D ssao_noise;

layout(location = 0) in highp vec2 in_ndc_texcoord;
layout(location = 0) out highp vec4 out_color;

void main() {
	highp vec2 texcoord = in_ndc_texcoord * 0.5 + 0.5;
    highp float depth = texture(in_depth, texcoord).r;
	if (depth == 1.0) {
		out_color = vec4(1.0);
		return;
	}
	    // 获取法线并解码（假设法线存储在[0,1]范围）
    highp vec3 normal = subpassLoad(in_normal).rgb * 2.0 - 1.0;
    normal = normalize(mat3(view) * normal);

    // 从噪声纹理获取随机旋转向量（假设噪声纹理重复平铺）
    highp vec2 noiseScale = vec2(windowWidth/8.0, windowHeight/8.0);
    highp vec3 randomVec = texture(ssao_noise, texcoord * noiseScale).xyz;

	highp float angle = fract(sin(dot(texcoord, vec2(12.9898, 78.233))) * 43758.5453) * 6.283185; // 生成随机角度
	highp vec3 tangent = vec3(cos(angle), sin(angle), 0.0);
	highp vec3 bitangent = cross(normal, tangent);
	highp mat3 TBN = mat3(tangent, bitangent, normal);

    // 创建TBN矩阵（切线->观察空间）
    //highp vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    //highp vec3 bitangent = cross(normal, tangent);
    //highp mat3 TBN = mat3(tangent, bitangent, normal);

    // 获取当前像素的观察空间位置
    highp vec4 clipPos = vec4(texcoord * 2.0 - 1.0, depth, 1.0);
    highp vec4 viewPos = inverse(projection) * clipPos;
    viewPos.xyz /= viewPos.w;

    // SSAO计算
    highp float occlusion = 0.0;
    int kernelSize = min(SSAOKernelSize, 64);
    for(int i = 0; i < kernelSize; ++i) {
        // 获取采样点并转换到观察空间
        highp vec3 samplePos = TBN * samples[i].xyz; // 切线->观察空间
        //samplePos = viewPos.xyz + normal * SSAORadius; 
        samplePos = viewPos.xyz + samplePos * SSAORadius; 

        // 投影到裁剪空间
        highp vec4 offset = projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;                 // 透视除法
        offset.xy = offset.xy * 0.5 + 0.5;      // 转换到[0,1]范围
        
        // 获取采样点深度值
        highp float sampleDepth = texture(in_depth, offset.xy).r;
        sampleDepth =  -projection[3][2] / (sampleDepth + projection[2][2]);

        // 范围检查和平滑处理
        highp float rangeCheck = smoothstep(0.0, 1.0, SSAORadius / abs(viewPos.z - sampleDepth));
        occlusion += ((sampleDepth >= (samplePos.z + SSAOBias)) ? 1.0 : 0.0)* rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(kernelSize));
    out_color = vec4(mix(vec3(1.0), vec3(occlusion), SSAOEffect), 1.0);

}
