#pragma once
#include "runtime/core/meta/reflection/reflection.h"

namespace Piccolo
{
    REFLECTION_TYPE(MaterialRes)
    CLASS(MaterialRes, Fields)
    {
        REFLECTION_BODY(MaterialRes);

    public:
        bool        m_is_NBR_material {false};
        std::string m_base_colour_texture_file;
        std::string m_metallic_roughness_texture_file;
        std::string m_normal_texture_file;
        std::string m_occlusion_texture_file;
        std::string m_emissive_texture_file;
        std::string m_light_map_texture_file;
        std::string m_ramp_warm_texture_file;
        std::string m_ramp_cool_texture_file;
        std::string m_face_map_texture_file;
        std::string m_nbr_material_setting_file;

    };

    REFLECTION_TYPE(MaterialSetting)
    CLASS(MaterialSetting, Fields)
    {
        REFLECTION_BODY(MaterialSetting);

    public:
        uint32_t m_nbr_mesh_id;
        uint32_t m_area; // 0 body 1 hair 2 face
        Vector3  m_front_face_tint_color;
        float    m_alpha;
        Vector3  m_back_face_tint_color;
        float    m_alpha_clip;
        float    m_indirect_light_usage;
        float    m_indirect_light_mix_base_color;
        float    m_indirect_light_occlusion_usage;
        float    m_main_light_color_usage;
        float    m_shadow_threshold_center;
        float    m_shadow_threshold_softness;
        float    m_shadow_ramp_offset;
        float    m_face_shadow_offset;
        float    m_face_shadow_transition_softness;
        float    m_specular_exponent;
        float    m_specular_Ks_non_metal;
        float    m_specular_Ks_metal;
        float    m_specular_brightness;
        float    m_rim_light_width;
        float    m_rim_light_threshold;
        float    m_rim_light_fadeout;
        float    m_rim_light_brightness;
        Vector3  m_rim_light_tint_color;
        float    m_rim_light_mix_albedo;
        Vector3  m_emission_tint_color;
        float    m_emission_mix_base_color;
        float    m_emission_intensity;

    };
} // namespace Piccolo