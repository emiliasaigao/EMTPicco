#pragma once

#include "runtime/core/math/matrix4.h"
#include "runtime/core/math/vector3.h"
#include "runtime/core/math/vector4.h"

#include "runtime/function/render/render_type.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace Piccolo
{
    static const uint32_t s_point_light_shadow_map_dimension       = 2048;
    static const uint32_t s_directional_light_shadow_map_dimension = 4096;

    // TODO: 64 may not be the best
    static uint32_t const s_mesh_per_drawcall_max_instance_count = 64;
    static uint32_t const s_mesh_vertex_blending_max_joint_count = 1024;
    static uint32_t const s_max_point_light_count                = 15;
    // should sync the macros in "shader_include/constants.h"

    struct VulkanSceneDirectionalLight
    {
        Vector3 direction;
        float   _padding_direction;
        Vector3 color;
        float   _padding_color;
    };

    struct VulkanScenePointLight
    {
        Vector3 position;
        float   radius;
        Vector3 intensity;
        float   _padding_intensity;
    };

    struct MeshPerframeStorageBufferObject
    {
        Matrix4x4                   proj_view_matrix;
        Vector3                     camera_position;
        float                       _padding_camera_position;
        Vector3                     ambient_light;
        float                       _padding_ambient_light;
        uint32_t                    point_light_num;
        uint32_t                    _padding_point_light_num_1;
        uint32_t                    _padding_point_light_num_2;
        uint32_t                    _padding_point_light_num_3;
        VulkanScenePointLight       scene_point_lights[s_max_point_light_count];
        VulkanSceneDirectionalLight scene_directional_light;
        Matrix4x4                   directional_light_proj_view;
    };

    struct NBRMeshPerframeStorageBufferObject
    {
        Matrix4x4                   proj_view_matrix;
        Matrix4x4                   proj_matrix;
        Matrix4x4                   view_matrix;
        Vector3                     camera_position;
        float                       _padding_camera_position;
        VulkanSceneDirectionalLight scene_directional_light;
    };
    
    struct NBROutlineMeshPerframeStorageBufferObject
    {
        Matrix4x4                   proj_view_matrix;
        Matrix4x4                   proj_matrix;
        Matrix4x4                   view_matrix;
        float                       cameraFOV;
        float                       _padding_outline_1;
        float                       _padding_outline_2;
        float                       _padding_outline_3;
    };

    struct VulkanMeshInstance
    {
        float     enable_vertex_blending;
        float     _padding_enable_vertex_blending_1;
        float     _padding_enable_vertex_blending_2;
        float     _padding_enable_vertex_blending_3;
        Matrix4x4 model_matrix;
    };

    struct MeshPerdrawcallStorageBufferObject
    {
        VulkanMeshInstance mesh_instances[s_mesh_per_drawcall_max_instance_count];
    };

    struct MeshPerdrawcallVertexBlendingStorageBufferObject
    {
        Matrix4x4 joint_matrices[s_mesh_vertex_blending_max_joint_count * s_mesh_per_drawcall_max_instance_count];
    };

    struct MeshPerMaterialUniformBufferObject
    {
        Vector4 baseColorFactor {0.0f, 0.0f, 0.0f, 0.0f};

        float metallicFactor    = 0.0f;
        float roughnessFactor   = 0.0f;
        float normalScale       = 0.0f;
        float occlusionStrength = 0.0f;

        Vector3  emissiveFactor  = {0.0f, 0.0f, 0.0f};
        uint32_t is_blend        = 0;
        uint32_t is_double_sided = 0;
    };

    struct MeshPerNBRMaterialUniformBufferObject
    {
        uint32_t _area;
        float    padding_area_1;
        float    padding_area_2;
        float    padding_area_3;

        Vector3 _front_face_tint_color;
        float   padding_front_face;
        Vector3 _back_face_tint_color;
        float   padding_back_face;

        float   _alpha;
        float   _alpha_clip;
        float   padding_alpha_1;
        float   padding_alpha_2;

        float   _indirect_light_usage;
        float   _indirect_light_mix_base_color;
        float   _indirect_light_occlusion_usage;
        float   _main_light_color_usage;
        float   _shadow_threshold_center;
        float   _shadow_threshold_softness;
        float   _shadow_ramp_offset;
        float   padding_lighting;

        float   _face_shadow_offset;
        float   _face_shadow_transition_softness;
        float   padding_face_shadow_1;
        float   padding_face_shadow_2;

        float   _specular_exponent;
        float   _specular_Ks_non_metal;
        float   _specular_Ks_metal;
        float   _specular_brightness;

        float   _rim_light_width;
        float   _rim_light_threshold;
        float   _rim_light_fadeout;
        float   _rim_light_brightness;

        Vector3 _rim_light_tint_color;
        float   padding_rim_light_tint;

        float   _rim_light_mix_albedo;
        float   padding_rim_light_mix_1;
        float   padding_rim_light_mix_2;
        float   padding_rim_light_mix_3;

        Vector3 _emission_tint_color;
        float   padding_emission_tint;

        float   _emission_mix_base_color;
        float   _emission_intensity;
        float   padding_emission_1;
        float   padding_emission_2;
    };

    struct MeshPointLightShadowPerframeStorageBufferObject
    {
        uint32_t point_light_num;
        uint32_t _padding_point_light_num_1;
        uint32_t _padding_point_light_num_2;
        uint32_t _padding_point_light_num_3;
        Vector4  point_lights_position_and_radius[s_max_point_light_count];
    };

    struct MeshPointLightShadowPerdrawcallStorageBufferObject
    {
        VulkanMeshInstance mesh_instances[s_mesh_per_drawcall_max_instance_count];
    };

    struct MeshPointLightShadowPerdrawcallVertexBlendingStorageBufferObject
    {
        Matrix4x4 joint_matrices[s_mesh_vertex_blending_max_joint_count * s_mesh_per_drawcall_max_instance_count];
    };

    struct RenderGlobalEffectSettingObject
    {
        // SSAO Effect
        //float SSAOEffect = 1.0;
        //float SSAORadius = 0.1;
        //float SSAOBias = 0.01;
        //int SSAOKernelSize = 32;
        float HBAOIntensity = 1.0f;
        float HBAORadius    = 1.625f;
        float HBAOMaxRadiusPixels = 64.f;
        float HBAOAngleBias       = 0.25f;

        // Bloom Effect
        float bloomThreshold = 1.5f;
        int   bloomBlurTimes = 5.f;
        int   bloomKernalSize = 4.f;

        // ColorGrading Effect
        float useColorGrading = 0.0f;
        float brightness      = 0.0f;
        float contrast        = 1.2f;
        float saturation      = 1.55f;
        float temperature     = 0.3f;       

        // Vignette Effect
        float vignetteCutoff = 0.54f;
        float vignetteExponent = 1.2f;
    };

    struct SSAOGeneratePerframeStorageBufferObject
    {
        Matrix4x4 proj_matrix;
        Matrix4x4 proj_inv_matrix;
        Matrix4x4 view_matrix;

        float intensity;
        float radius;
        float max_radius_pixels;
        float angle_bias;

        float radius_pixel;
        float neg_inv_radius2;
        float windowWidth;
        float windowHeight;

        /* Matrix4x4 projection;       
        Matrix4x4   view;             
        Vector4     samples[64];        

        float       windowWidth;    
        float       windowHeight;   
        float       SSAOEffect;       
        float       SSAORadius;    

        float       SSAOBias;
        int32_t     SSAOKernelSize;   
        uint32_t    _padding[2];      */
    };

    struct MeshDirectionalLightShadowPerframeStorageBufferObject
    {
        Matrix4x4 light_proj_view;
    };

    struct MeshDirectionalLightShadowPerdrawcallStorageBufferObject
    {
        VulkanMeshInstance mesh_instances[s_mesh_per_drawcall_max_instance_count];
    };

    struct MeshDirectionalLightShadowPerdrawcallVertexBlendingStorageBufferObject
    {
        Matrix4x4 joint_matrices[s_mesh_vertex_blending_max_joint_count * s_mesh_per_drawcall_max_instance_count];
    };

    struct AxisStorageBufferObject
    {
        Matrix4x4 model_matrix  = Matrix4x4::IDENTITY;
        uint32_t  selected_axis = 3;
    };

    struct ParticleBillboardPerframeStorageBufferObject
    {
        Matrix4x4 proj_view_matrix;
        Vector3   right_direction;
        float     _padding_right_position;
        Vector3   up_direction;
        float     _padding_up_direction;
        Vector3   foward_direction;
        float     _padding_forward_position;
    };

    struct ParticleCollisionPerframeStorageBufferObject
    {
        Matrix4x4 view_matrix;
        Matrix4x4 proj_view_matrix;
        Matrix4x4 proj_inv_matrix;
    };

    // TODO: 4096 may not be the best
    static constexpr int s_particle_billboard_buffer_size = 4096;
    struct ParticleBillboardPerdrawcallStorageBufferObject
    {
        Vector4 positions[s_particle_billboard_buffer_size];
        Vector4 sizes[s_particle_billboard_buffer_size];
        Vector4 colors[s_particle_billboard_buffer_size];
    };

    struct MeshInefficientPickPerframeStorageBufferObject
    {
        Matrix4x4 proj_view_matrix;
        uint32_t  rt_width;
        uint32_t  rt_height;
    };

    struct MeshInefficientPickPerdrawcallStorageBufferObject
    {
        Matrix4x4 model_matrices[s_mesh_per_drawcall_max_instance_count];
        uint32_t  node_ids[s_mesh_per_drawcall_max_instance_count];
        float     enable_vertex_blendings[s_mesh_per_drawcall_max_instance_count];
    };

    struct MeshInefficientPickPerdrawcallVertexBlendingStorageBufferObject
    {
        Matrix4x4 joint_matrices[s_mesh_vertex_blending_max_joint_count * s_mesh_per_drawcall_max_instance_count];
    };

    // mesh
    struct VulkanMesh
    {
        bool enable_vertex_blending;

        uint32_t mesh_vertex_count;

        VkBuffer      mesh_vertex_position_buffer;
        VmaAllocation mesh_vertex_position_buffer_allocation;

        VkBuffer      mesh_vertex_varying_enable_blending_buffer;
        VmaAllocation mesh_vertex_varying_enable_blending_buffer_allocation;

        VkBuffer      mesh_vertex_joint_binding_buffer;
        VmaAllocation mesh_vertex_joint_binding_buffer_allocation;

        VkDescriptorSet mesh_vertex_blending_descriptor_set;

        VkBuffer      mesh_vertex_varying_buffer;
        VmaAllocation mesh_vertex_varying_buffer_allocation;

        VkBuffer      mesh_vertex_color_buffer;
        VmaAllocation mesh_vertex_color_buffer_allocation;

        uint32_t mesh_index_count;

        VkBuffer      mesh_index_buffer;
        VmaAllocation mesh_index_buffer_allocation;
    };

    struct VulkanMaterial
    {};

    struct VulkanNBRMaterial : public VulkanMaterial
    {
        VkImage       base_color_texture_image = VK_NULL_HANDLE;
        VkImageView   base_color_image_view    = VK_NULL_HANDLE;
        VmaAllocation base_color_image_allocation;

        VkImage       light_map_texture_image = VK_NULL_HANDLE;
        VkImageView   light_map_texture_image_view = VK_NULL_HANDLE;
        VmaAllocation light_map_texture_image_allocation;

        VkImage       ramp_warm_texture_image = VK_NULL_HANDLE;
        VkImageView   ramp_warm_texture_view    = VK_NULL_HANDLE;
        VmaAllocation ramp_warm_texture_allocation;

        VkImage       ramp_cool_texture_image = VK_NULL_HANDLE;
        VkImageView   ramp_cool_texture_view    = VK_NULL_HANDLE;
        VmaAllocation ramp_cool_texture_allocation;

        VkImage       face_map_texture_image = VK_NULL_HANDLE;
        VkImageView   face_map_view    = VK_NULL_HANDLE;
        VmaAllocation face_map_allocation;

        VkBuffer      material_uniform_buffer;
        VmaAllocation material_uniform_buffer_allocation;

        VkDescriptorSet material_descriptor_set;
    };

    // material
    struct VulkanPBRMaterial : public VulkanMaterial
    {
        VkImage       base_color_texture_image = VK_NULL_HANDLE;
        VkImageView   base_color_image_view    = VK_NULL_HANDLE;
        VmaAllocation base_color_image_allocation;

        VkImage       metallic_roughness_texture_image = VK_NULL_HANDLE;
        VkImageView   metallic_roughness_image_view    = VK_NULL_HANDLE;
        VmaAllocation metallic_roughness_image_allocation;

        VkImage       normal_texture_image = VK_NULL_HANDLE;
        VkImageView   normal_image_view    = VK_NULL_HANDLE;
        VmaAllocation normal_image_allocation;

        VkImage       occlusion_texture_image = VK_NULL_HANDLE;
        VkImageView   occlusion_image_view    = VK_NULL_HANDLE;
        VmaAllocation occlusion_image_allocation;

        VkImage       emissive_texture_image = VK_NULL_HANDLE;
        VkImageView   emissive_image_view    = VK_NULL_HANDLE;
        VmaAllocation emissive_image_allocation;

        VkBuffer      material_uniform_buffer;
        VmaAllocation material_uniform_buffer_allocation;

        VkDescriptorSet material_descriptor_set;
    };

    // nodes
    struct RenderMeshNode
    {
        const Matrix4x4*   model_matrix {nullptr};
        const Matrix4x4*   joint_matrices {nullptr};
        uint32_t           joint_count {0};
        VulkanMesh*        ref_mesh {nullptr};
        VulkanPBRMaterial* ref_material {nullptr};
        VulkanNBRMaterial* ref_material_nbr {nullptr};
        uint32_t           node_id;
        uint32_t           nbr_mesh_id {10000};
        bool               is_NBR_material {false};
        bool               enable_vertex_blending {false};
    };

    struct RenderAxisNode
    {
        Matrix4x4   model_matrix {Matrix4x4::IDENTITY};
        VulkanMesh* ref_mesh {nullptr};
        uint32_t    node_id;
        bool        enable_vertex_blending {false};
    };

    struct TextureDataToUpdate
    {
        void*                base_color_image_pixels;
        uint32_t             base_color_image_width;
        uint32_t             base_color_image_height;
        PICCOLO_PIXEL_FORMAT base_color_image_format;
        void*                metallic_roughness_image_pixels;
        uint32_t             metallic_roughness_image_width;
        uint32_t             metallic_roughness_image_height;
        PICCOLO_PIXEL_FORMAT metallic_roughness_image_format;
        void*                normal_roughness_image_pixels;
        uint32_t             normal_roughness_image_width;
        uint32_t             normal_roughness_image_height;
        PICCOLO_PIXEL_FORMAT normal_roughness_image_format;
        void*                occlusion_image_pixels;
        uint32_t             occlusion_image_width;
        uint32_t             occlusion_image_height;
        PICCOLO_PIXEL_FORMAT occlusion_image_format;
        void*                emissive_image_pixels;
        uint32_t             emissive_image_width;
        uint32_t             emissive_image_height;
        PICCOLO_PIXEL_FORMAT emissive_image_format;
        VulkanPBRMaterial*   now_material;
    };

    struct TextureDataToUpdateNBR
    {
        void*                base_color_image_pixels;
        uint32_t             base_color_image_width;
        uint32_t             base_color_image_height;
        PICCOLO_PIXEL_FORMAT base_color_image_format;
        void*                light_map_image_pixels;
        uint32_t             light_map_image_width;
        uint32_t             light_map_image_height;
        PICCOLO_PIXEL_FORMAT light_map_image_format;
        void*                ramp_warm_image_pixels;
        uint32_t             ramp_warm_image_width;
        uint32_t             ramp_warm_image_height;
        PICCOLO_PIXEL_FORMAT ramp_warm_image_format;
        void*                ramp_cool_image_pixels;
        uint32_t             ramp_cool_image_width;
        uint32_t             ramp_cool_image_height;
        PICCOLO_PIXEL_FORMAT ramp_cool_image_format;
        void*                face_map_image_pixels;
        uint32_t             face_map_image_width;
        uint32_t             face_map_image_height;
        PICCOLO_PIXEL_FORMAT face_map_image_format;
        VulkanNBRMaterial*   now_material;
    };
} // namespace Piccolo
