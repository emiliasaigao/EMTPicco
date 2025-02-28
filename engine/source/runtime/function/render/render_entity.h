#pragma once

#include "runtime/core/math/axis_aligned.h"
#include "runtime/core/math/matrix4.h"

#include <cstdint>
#include <vector>

namespace Piccolo
{
    class RenderEntity
    {
    public:
        uint32_t  m_instance_id {0};
        Matrix4x4 m_model_matrix {Matrix4x4::IDENTITY};

        // mesh
        size_t                 m_mesh_asset_id {0};
        bool                   m_enable_vertex_blending {false};
        std::vector<Matrix4x4> m_joint_matrices;
        AxisAlignedBox         m_bounding_box;
        uint32_t               m_nbr_mesh_id {10000};

        // material
        bool    m_is_NBR_material {false};
        size_t  m_material_asset_id {0};
        bool    m_blend {false};
        bool    m_double_sided {false};
        Vector4 m_base_color_factor {1.0f, 1.0f, 1.0f, 1.0f};
        float   m_metallic_factor {1.0f};
        float   m_roughness_factor {1.0f};
        float   m_normal_scale {1.0f};
        float   m_occlusion_strength {1.0f};
        Vector3 m_emissive_factor {1.0f, 1.0f, 1.0f};

        uint32_t m_area; // 0 body, 1 hair, 2 face
        Vector3 m_front_face_tint_color;
        float   m_alpha;
        Vector3 m_back_face_tint_color;
        float   m_alpha_clip;
        float m_indirect_light_usage;
        float m_indirect_light_mix_base_color;
        float m_indirect_light_occlusion_usage;
        float m_main_light_color_usage;
        float m_shadow_threshold_center;
        float m_shadow_threshold_softness;
        float m_shadow_ramp_offset;
        float m_face_shadow_offset;
        float m_face_shadow_transition_softness;
        float m_specular_exponent;
        float m_specular_Ks_non_metal;
        float m_specular_Ks_metal;
        float m_specular_brightness;
        float   m_rim_light_width;
        float   m_rim_light_threshold;
        float   m_rim_light_fadeout;
        float   m_rim_light_brightness;
        Vector3 m_rim_light_tint_color;
        float   m_rim_light_mix_albedo;
        Vector3 m_emission_tint_color;
        float   m_emission_mix_base_color;
        float   m_emission_intensity;
    };
} // namespace Piccolo
