#include "runtime/function/framework/component/mesh/mesh_component.h"

#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/res_type/data/material.h"

#include "runtime/function/framework/component/animation/animation_component.h"
#include "runtime/function/framework/component/transform/transform_component.h"
#include "runtime/function/framework/object/object.h"
#include "runtime/function/global/global_context.h"

#include "runtime/function/render/render_swap_context.h"
#include "runtime/function/render/render_system.h"

namespace Piccolo
{
    void MeshComponent::postLoadResource(std::weak_ptr<GObject> parent_object)
    {
        m_parent_object = parent_object;

        std::shared_ptr<AssetManager> asset_manager = g_runtime_global_context.m_asset_manager;
        ASSERT(asset_manager);

        m_raw_meshes.resize(m_mesh_res.m_sub_meshes.size());

        size_t raw_mesh_count = 0;
        for (const SubMeshRes& sub_mesh : m_mesh_res.m_sub_meshes)
        {
            GameObjectPartDesc& meshComponent = m_raw_meshes[raw_mesh_count];
            meshComponent.m_mesh_desc.m_mesh_file =
                asset_manager->getFullPath(sub_mesh.m_obj_file_ref).generic_string();

            meshComponent.m_material_desc.m_with_texture = sub_mesh.m_material.empty() == false;

            if (meshComponent.m_material_desc.m_with_texture)
            {
                MaterialRes material_res;
                asset_manager->loadAsset(sub_mesh.m_material, material_res);
                meshComponent.m_material_desc.m_is_NBR_material = material_res.m_is_NBR_material;
                if (material_res.m_is_NBR_material)
                {
                    MaterialSetting material_setting;
                    asset_manager->loadAsset(material_res.m_nbr_material_setting_file, material_setting);
                    meshComponent.m_material_desc.m_base_color_texture_file =
                        material_res.m_base_colour_texture_file.empty() ? "" :
                        asset_manager->getFullPath(material_res.m_base_colour_texture_file).generic_string();
                    meshComponent.m_material_desc.m_light_map_texture_file =
                        material_res.m_light_map_texture_file.empty() ? "" :
                        asset_manager->getFullPath(material_res.m_light_map_texture_file).generic_string();
                    meshComponent.m_material_desc.m_face_map_texture_file =
                        material_res.m_face_map_texture_file.empty() ? "" :
                        asset_manager->getFullPath(material_res.m_face_map_texture_file).generic_string();
                    meshComponent.m_material_desc.m_ramp_warm_texture_file =
                        material_res.m_ramp_warm_texture_file.empty() ? "" :
                        asset_manager->getFullPath(material_res.m_ramp_warm_texture_file).generic_string();
                    meshComponent.m_material_desc.m_ramp_cool_texture_file =
                        material_res.m_ramp_cool_texture_file.empty() ? "" :
                        asset_manager->getFullPath(material_res.m_ramp_cool_texture_file).generic_string();
                    meshComponent.m_material_desc.m_nbr_material_setting_file =
                        material_res.m_nbr_material_setting_file.empty() ? "" :
                            asset_manager->getFullPath(material_res.m_nbr_material_setting_file).generic_string();

                    meshComponent.m_material_desc.m_nbr_mesh_id                       = material_setting.m_nbr_mesh_id;
                    meshComponent.m_material_desc.m_area                              = material_setting.m_area;
                    meshComponent.m_material_desc.m_front_face_tint_color             = material_setting.m_front_face_tint_color;
                    meshComponent.m_material_desc.m_alpha                             = material_setting.m_alpha;
                    meshComponent.m_material_desc.m_back_face_tint_color              = material_setting.m_back_face_tint_color;
                    meshComponent.m_material_desc.m_alpha_clip                        = material_setting.m_alpha_clip;
                    meshComponent.m_material_desc.m_indirect_light_usage              = material_setting.m_indirect_light_usage;
                    meshComponent.m_material_desc.m_indirect_light_mix_base_color     = material_setting.m_indirect_light_mix_base_color;
                    meshComponent.m_material_desc.m_indirect_light_occlusion_usage    = material_setting.m_indirect_light_occlusion_usage;
                    meshComponent.m_material_desc.m_main_light_color_usage            = material_setting.m_main_light_color_usage;
                    meshComponent.m_material_desc.m_shadow_threshold_center           = material_setting.m_shadow_threshold_center;
                    meshComponent.m_material_desc.m_shadow_threshold_softness         = material_setting.m_shadow_threshold_softness;
                    meshComponent.m_material_desc.m_shadow_ramp_offset                = material_setting.m_shadow_ramp_offset;
                    meshComponent.m_material_desc.m_face_shadow_offset                = material_setting.m_face_shadow_offset;
                    meshComponent.m_material_desc.m_face_shadow_transition_softness   = material_setting.m_face_shadow_transition_softness;
                    meshComponent.m_material_desc.m_specular_exponent                 = material_setting.m_specular_exponent;
                    meshComponent.m_material_desc.m_specular_Ks_non_metal             = material_setting.m_specular_Ks_non_metal;
                    meshComponent.m_material_desc.m_specular_Ks_metal                 = material_setting.m_specular_Ks_metal;
                    meshComponent.m_material_desc.m_specular_brightness               = material_setting.m_specular_brightness;
                    meshComponent.m_material_desc.m_rim_light_width                   = material_setting.m_rim_light_width;
                    meshComponent.m_material_desc.m_rim_light_threshold               = material_setting.m_rim_light_threshold;
                    meshComponent.m_material_desc.m_rim_light_fadeout                 = material_setting.m_rim_light_fadeout;
                    meshComponent.m_material_desc.m_rim_light_brightness              = material_setting.m_rim_light_brightness;
                    meshComponent.m_material_desc.m_rim_light_tint_color              = material_setting.m_rim_light_tint_color;
                    meshComponent.m_material_desc.m_rim_light_mix_albedo              = material_setting.m_rim_light_mix_albedo;
                    meshComponent.m_material_desc.m_emission_tint_color               = material_setting.m_emission_tint_color;
                    meshComponent.m_material_desc.m_emission_mix_base_color           = material_setting.m_emission_mix_base_color;
                    meshComponent.m_material_desc.m_emission_intensity                = material_setting.m_emission_intensity;
                }
                else
                {
                    meshComponent.m_material_desc.m_base_color_texture_file =
                        asset_manager->getFullPath(material_res.m_base_colour_texture_file).generic_string();
                    meshComponent.m_material_desc.m_metallic_roughness_texture_file =
                        asset_manager->getFullPath(material_res.m_metallic_roughness_texture_file).generic_string();
                    meshComponent.m_material_desc.m_normal_texture_file =
                        asset_manager->getFullPath(material_res.m_normal_texture_file).generic_string();
                    meshComponent.m_material_desc.m_occlusion_texture_file =
                        asset_manager->getFullPath(material_res.m_occlusion_texture_file).generic_string();
                    meshComponent.m_material_desc.m_emissive_texture_file =
                        asset_manager->getFullPath(material_res.m_emissive_texture_file).generic_string();
                }
            }

            auto object_space_transform = sub_mesh.m_transform.getMatrix();

            meshComponent.m_transform_desc.m_transform_matrix = object_space_transform;

            ++raw_mesh_count;
        }
    }

    void MeshComponent::tick(float delta_time)
    {
        if (!m_parent_object.lock())
            return;

        TransformComponent*       transform_component = m_parent_object.lock()->tryGetComponent(TransformComponent);
        const AnimationComponent* animation_component =
            m_parent_object.lock()->tryGetComponentConst(AnimationComponent);

        if (transform_component->isDirty())
        {
            std::vector<GameObjectPartDesc> dirty_mesh_parts;
            SkeletonAnimationResult         animation_result;
            animation_result.m_transforms.push_back({Matrix4x4::IDENTITY});
            if (animation_component != nullptr)
            {
                for (auto& node : animation_component->getResult().node)
                {
                    animation_result.m_transforms.push_back({Matrix4x4(node.transform)});
                }
            }
            for (GameObjectPartDesc& mesh_part : m_raw_meshes)
            {
                if (animation_component)
                {
                    mesh_part.m_with_animation                                = true;
                    mesh_part.m_skeleton_animation_result                     = animation_result;
                    mesh_part.m_skeleton_binding_desc.m_skeleton_binding_file = mesh_part.m_mesh_desc.m_mesh_file;
                }
                Matrix4x4 object_transform_matrix = mesh_part.m_transform_desc.m_transform_matrix;

                mesh_part.m_transform_desc.m_transform_matrix =
                    transform_component->getMatrix() * object_transform_matrix;
                dirty_mesh_parts.push_back(mesh_part);

                mesh_part.m_transform_desc.m_transform_matrix = object_transform_matrix;
            }

            RenderSwapContext& render_swap_context = g_runtime_global_context.m_render_system->getSwapContext();
            RenderSwapData&    logic_swap_data     = render_swap_context.getLogicSwapData();

            logic_swap_data.addDirtyGameObject(GameObjectDesc {m_parent_object.lock()->getID(), dirty_mesh_parts});

            transform_component->setDirtyFlag(false);
        }
    }
} // namespace Piccolo
