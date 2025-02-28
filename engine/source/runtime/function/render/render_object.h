#pragma once

#include "runtime/core/math/matrix4.h"
#include "runtime/function/framework/object/object_id_allocator.h"

#include <string>
#include <vector>

namespace Piccolo
{
    REFLECTION_TYPE(GameObjectMeshDesc)
    STRUCT(GameObjectMeshDesc, Fields)
    {
        REFLECTION_BODY(GameObjectMeshDesc)
        std::string m_mesh_file;
    };

    REFLECTION_TYPE(SkeletonBindingDesc)
    STRUCT(SkeletonBindingDesc, Fields)
    {
        REFLECTION_BODY(SkeletonBindingDesc)
        std::string m_skeleton_binding_file;
    };

    REFLECTION_TYPE(SkeletonAnimationResultTransform)
    STRUCT(SkeletonAnimationResultTransform, WhiteListFields)
    {
        REFLECTION_BODY(SkeletonAnimationResultTransform)
        Matrix4x4 m_matrix;
    };

    REFLECTION_TYPE(SkeletonAnimationResult)
    STRUCT(SkeletonAnimationResult, Fields)
    {
        REFLECTION_BODY(SkeletonAnimationResult)
        std::vector<SkeletonAnimationResultTransform> m_transforms;
    };

    REFLECTION_TYPE(GameObjectMaterialDesc)
    STRUCT(GameObjectMaterialDesc, Fields)
    {
        REFLECTION_BODY(GameObjectMaterialDesc)
        std::string m_base_color_texture_file;
        std::string m_metallic_roughness_texture_file;
        std::string m_normal_texture_file;
        std::string m_occlusion_texture_file;
        std::string m_emissive_texture_file;
        std::string m_light_map_texture_file;
        std::string m_ramp_warm_texture_file;
        std::string m_ramp_cool_texture_file;
        std::string m_face_map_texture_file;
        std::string m_nbr_material_setting_file;

        // 0eyes 1eyebrows 2face 3mouth 4body 5hair 6eyeblack
        uint32_t    m_nbr_mesh_id;
        // 0 body 1 hair 2 face
        uint32_t    m_area; 
        Vector3     m_front_face_tint_color;
        float       m_alpha;
        Vector3     m_back_face_tint_color;
        float       m_alpha_clip;
        float       m_indirect_light_usage;
        float       m_indirect_light_mix_base_color;
        float       m_indirect_light_occlusion_usage;
        float       m_main_light_color_usage;
        float       m_shadow_threshold_center;
        float       m_shadow_threshold_softness;
        float       m_shadow_ramp_offset;
        float       m_face_shadow_offset;
        float       m_face_shadow_transition_softness;
        float       m_specular_exponent;
        float       m_specular_Ks_non_metal;
        float       m_specular_Ks_metal;
        float       m_specular_brightness;
        float       m_rim_light_width;
        float       m_rim_light_threshold;
        float       m_rim_light_fadeout;
        float       m_rim_light_brightness;
        Vector3     m_rim_light_tint_color;
        float       m_rim_light_mix_albedo;
        Vector3     m_emission_tint_color;
        float       m_emission_mix_base_color;
        float       m_emission_intensity;

        bool        m_is_NBR_material {true};
        bool        m_with_texture {false};
    };

    REFLECTION_TYPE(GameObjectTransformDesc)
    STRUCT(GameObjectTransformDesc, WhiteListFields)
    {
        REFLECTION_BODY(GameObjectTransformDesc)
        Matrix4x4 m_transform_matrix {Matrix4x4::IDENTITY};
    };

    REFLECTION_TYPE(GameObjectPartDesc)
    STRUCT(GameObjectPartDesc, Fields)
    {
        REFLECTION_BODY(GameObjectPartDesc)
        GameObjectMeshDesc      m_mesh_desc;
        GameObjectMaterialDesc  m_material_desc;
        GameObjectTransformDesc m_transform_desc;
        bool                    m_with_animation {false};
        SkeletonBindingDesc     m_skeleton_binding_desc;
        SkeletonAnimationResult m_skeleton_animation_result;
    };

    constexpr size_t k_invalid_part_id = std::numeric_limits<size_t>::max();

    struct GameObjectPartId
    {
        GObjectID m_go_id {k_invalid_gobject_id};
        size_t    m_part_id {k_invalid_part_id};

        bool   operator==(const GameObjectPartId& rhs) const { return m_go_id == rhs.m_go_id && m_part_id == rhs.m_part_id; }
        size_t getHashValue() const { return m_go_id ^ (m_part_id << 1); }
        bool   isValid() const { return m_go_id != k_invalid_gobject_id && m_part_id != k_invalid_part_id; }
    };

    class GameObjectDesc
    {
    public:
        GameObjectDesc() : m_go_id(0) {}
        GameObjectDesc(size_t go_id, const std::vector<GameObjectPartDesc>& parts) :
            m_go_id(go_id), m_object_parts(parts)
        {}

        GObjectID                              getId() const { return m_go_id; }
        const std::vector<GameObjectPartDesc>& getObjectParts() const { return m_object_parts; }

    private:
        GObjectID                       m_go_id {k_invalid_gobject_id};
        std::vector<GameObjectPartDesc> m_object_parts;
    };
} // namespace Piccolo

template<>
struct std::hash<Piccolo::GameObjectPartId>
{
    size_t operator()(const Piccolo::GameObjectPartId& rhs) const noexcept { return rhs.getHashValue(); }
};
