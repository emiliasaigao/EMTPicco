#version 310 es

#extension GL_GOOGLE_include_directive : enable

#include "constants.h"
#include "structures.h"

struct DirectionalLight
{
    highp vec3  direction;
    lowp float _padding_direction;
    highp vec3  color;
    lowp float _padding_color;
};


layout(set = 0, binding = 0) readonly buffer _unused_name_perframe
{
    highp mat4   proj_view_matrix;
    highp mat4   proj_matrix;
    highp mat4   view_matrix;
    highp vec3   camera_position;
    lowp float   _padding_camera_position;
    DirectionalLight scene_directional_light;
};

layout(set = 0, binding = 1) readonly buffer _unused_name_per_drawcall
{
    VulkanMeshInstance mesh_instances[m_mesh_per_drawcall_max_instance_count];
};

layout(set = 0, binding = 2) readonly buffer _unused_name_per_drawcall_vertex_blending
{
    highp mat4 joint_matrices[m_mesh_vertex_blending_max_joint_count * m_mesh_per_drawcall_max_instance_count];
};
layout(set = 1, binding = 0) readonly buffer _unused_name_per_mesh_joint_binding
{
    VulkanMeshVertexJointBinding indices_and_weights[];
};

layout(location = 0) in vec3 in_position; // for some types as dvec3 takes 2 locations
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_tangent;
layout(location = 3) in vec2 in_texcoord;

layout(location = 0) out vec3 out_world_position; // output in framebuffer 0 for fragment shader
layout(location = 1) out vec4 out_clip_position;
layout(location = 2) out vec3 out_normal;
layout(location = 3) out vec3 out_view_dir;
layout(location = 4) out vec2 out_texcoord;

void main()
{
    highp mat4  model_matrix           = mesh_instances[gl_InstanceIndex].model_matrix;
    highp float enable_vertex_blending = mesh_instances[gl_InstanceIndex].enable_vertex_blending;

    highp vec3 model_position;
    highp vec3 model_normal;
    if (enable_vertex_blending > 0.0)
    {
        highp ivec4 in_indices = indices_and_weights[gl_VertexIndex].indices;
        highp vec4  in_weights = indices_and_weights[gl_VertexIndex].weights;

        highp mat4 vertex_blending_matrix = mat4x4(
            vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0));

        if (in_weights.x > 0.0 && in_indices.x > 0)
        {
            vertex_blending_matrix +=
                joint_matrices[m_mesh_vertex_blending_max_joint_count * gl_InstanceIndex + in_indices.x] * in_weights.x;
        }

        if (in_weights.y > 0.0 && in_indices.y > 0)
        {
            vertex_blending_matrix +=
                joint_matrices[m_mesh_vertex_blending_max_joint_count * gl_InstanceIndex + in_indices.y] * in_weights.y;
        }

        if (in_weights.z > 0.0 && in_indices.z > 0)
        {
            vertex_blending_matrix +=
                joint_matrices[m_mesh_vertex_blending_max_joint_count * gl_InstanceIndex + in_indices.z] * in_weights.z;
        }

        if (in_weights.w > 0.0 && in_indices.w > 0)
        {
            vertex_blending_matrix +=
                joint_matrices[m_mesh_vertex_blending_max_joint_count * gl_InstanceIndex + in_indices.w] * in_weights.w;
        }

        model_position = (vertex_blending_matrix * vec4(in_position, 1.0)).xyz;

        highp mat3x3 vertex_blending_tangent_matrix =
            mat3x3(vertex_blending_matrix[0].xyz, vertex_blending_matrix[1].xyz, vertex_blending_matrix[2].xyz);

        model_normal  = normalize(vertex_blending_tangent_matrix * in_normal);
        
    }
    else
    {
        model_position = in_position;
        model_normal   = in_normal;
        
    }

    out_world_position = (model_matrix * vec4(model_position, 1.0)).xyz;

    out_clip_position = proj_view_matrix * vec4(out_world_position, 1.0f);

    gl_Position = out_clip_position;

    // TODO: normal matrix
    mat3x3 tangent_matrix = transpose(inverse(mat3(model_matrix)));
    out_normal            = normalize(tangent_matrix * model_normal);
    out_view_dir          = normalize(camera_position - out_world_position);

    out_texcoord = in_texcoord;
}
