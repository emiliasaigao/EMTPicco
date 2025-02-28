// todo: param/const
#define MAX_REFLECTION_LOD 8.0
#define FILTER_STRIDE 5.0
#define NUM_SAMPLES 100 // 采样数
#define NUM_RINGS 10 // 泊松圆盘采样参数
highp vec2 poissonDisk[NUM_SAMPLES]; // 泊松圆盘采样结果

// Normal Distribution function --------------------------------------
highp float D_GGX(highp float dotNH, highp float roughness)
{
    highp float alpha  = roughness * roughness;
    highp float alpha2 = alpha * alpha;
    highp float denom  = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
    return (alpha2) / (PI * denom * denom);
}

// Geometric Shadowing function --------------------------------------
highp float G_SchlicksmithGGX(highp float dotNL, highp float dotNV, highp float roughness)
{
    highp float r  = (roughness + 1.0);
    highp float k  = (r * r) / 8.0;
    highp float GL = dotNL / (dotNL * (1.0 - k) + k);
    highp float GV = dotNV / (dotNV * (1.0 - k) + k);
    return GL * GV;
}

// Fresnel function ----------------------------------------------------
highp float Pow5(highp float x)
{
    return (x * x * x * x * x);
}

highp vec3 F_Schlick(highp float cosTheta, highp vec3 F0) 
{ 
    return F0 + (1.0 - F0) * Pow5(1.0 - cosTheta); 
    }

highp vec3 F_SchlickR(highp float cosTheta, highp vec3 F0, highp float roughness)
{
    return F0 + (max(vec3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * Pow5(1.0 - cosTheta);
}

// Specular and diffuse BRDF composition --------------------------------------------
highp vec3 BRDF(highp vec3  L,
                highp vec3  V,
                highp vec3  N,
                highp vec3  F0,
                highp vec3  basecolor,
                highp float metallic,
                highp float roughness)
{
    // Precalculate vectors and dot products
    highp vec3  H     = normalize(V + L);
    highp float dotNV = clamp(dot(N, V), 0.0, 1.0);
    highp float dotNL = clamp(dot(N, L), 0.0, 1.0);
    highp float dotLH = clamp(dot(L, H), 0.0, 1.0);
    highp float dotNH = clamp(dot(N, H), 0.0, 1.0);

    // Light color fixed
    // vec3 lightColor = vec3(1.0);

    highp vec3 color = vec3(0.0);

    highp float rroughness = max(0.05, roughness);
    // D = Normal distribution (Distribution of the microfacets)
    highp float D = D_GGX(dotNH, rroughness);
    // G = Geometric shadowing term (Microfacets shadowing)
    highp float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
    // F = Fresnel factor (Reflectance depending on angle of incidence)
    highp vec3 F = F_Schlick(dotNV, F0);

    highp vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);
    highp vec3 kD   = (vec3(1.0) - F) * (1.0 - metallic);

    color += (kD * basecolor / PI + (1.0 - kD) * spec);
    // color += (kD * basecolor / PI + spec) * dotNL;
    // color += (kD * basecolor / PI + spec) * dotNL * lightColor;

    return color;
}

highp vec2 ndcxy_to_uv(highp vec2 ndcxy) { return ndcxy * vec2(0.5, 0.5) + vec2(0.5, 0.5); }

highp vec2 uv_to_ndcxy(highp vec2 uv) { return uv * vec2(2.0, 2.0) + vec2(-1.0, -1.0); }

// 给1维返1维
highp float rand_1to1(highp float x) {
	// -1 -1
	return fract(sin(x) * 10000.0);
}

// 给2维返一维
highp float rand_2to1(highp vec2 uv) {
	highp float dt = dot(uv.xy, vec2(12.9898, 78.233)), sn = mod(dt, PI);
	return fract(sin(sn) * 43758.5453);
}

void poissonDiskSamples(highp vec2 randomSeed) {
	highp float ANGLE_STEP = PI2 * float(NUM_RINGS) / float(NUM_SAMPLES);
	highp float INV_NUM_SAMPLES = 1.0 / float(NUM_SAMPLES);

	highp float angle = rand_2to1(randomSeed) * PI2;
	highp float radius = INV_NUM_SAMPLES;
	highp float radiusStep = radius;

	for (int i = 0; i < NUM_SAMPLES; i++) {
		poissonDisk[i] = vec2(cos(angle), sin(angle)) * pow(radius, 0.75);
		radius += radiusStep;
		angle += ANGLE_STEP;
	}
}

highp float PCF(highp vec3 coords, highp float filterRadiusUV) {
	highp vec2 uv = coords.xy;
	highp float cur_depth = coords.z; //当前深度

	highp float bias = 0.000075;

	// 在当前像素周围采样NUM_SAMPLES个像素，计算遮挡百分比
	highp float count = 0.;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		highp vec2 sample_uv = uv + poissonDisk[i] * filterRadiusUV;
		highp float ldepth = texture(directional_light_shadow, sample_uv).r;
		if (ldepth > 0.99f) {
			count += 1.;
			continue;
		}
		count += (ldepth) >= cur_depth - bias ? 1. : 0.;
	}

	return count / float(NUM_SAMPLES);
}