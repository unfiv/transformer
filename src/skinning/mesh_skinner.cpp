#include "skinning/mesh_skinner.hpp"

#include "core/math_utils.hpp"

#include <stdexcept>

namespace transformer
{

Mesh MeshSkinner::skin(const Mesh& source_mesh, const BoneWeightsData& bone_weights_data, const BonePoseData& bone_pose_data,
                       Profiler& profiler) const
{
    const auto scope = profiler.stage("cpu_skinning");

    if (source_mesh.positions.size() != bone_weights_data.per_vertex_weights.size())
    {
        throw std::runtime_error("Vertex count mismatch between mesh and skinning weights");
    }

    if (bone_pose_data.inverse_bind_pose.size() != bone_pose_data.new_pose.size())
    {
        throw std::runtime_error("Bone count mismatch between inverse bind pose and new pose");
    }

    Mesh result_mesh = source_mesh;

    for (std::size_t vertex_index = 0; vertex_index < source_mesh.positions.size(); ++vertex_index)
    {
        const Vec3& source_position = source_mesh.positions[vertex_index];
        const VertexBoneWeights& vertex_bone_weights = bone_weights_data.per_vertex_weights[vertex_index];

        Vec4 blended_position{ 0.0F, 0.0F, 0.0F, 0.0F };
        const Vec4 source_position_h{ source_position.x, source_position.y, source_position.z, 1.0F };
        float accumulated_weight = 0.0F;

        for (std::size_t i = 0; i < 4; ++i)
        {
            const std::int8_t bone_index = vertex_bone_weights.bone_indices[i];
            const float weight = vertex_bone_weights.weights[i];

            if (bone_index < 0 || weight <= 0.0F)
            {
                continue;
            }

            const Mat4 skinning_matrix =
                multiply(bone_pose_data.new_pose[bone_index], bone_pose_data.inverse_bind_pose[bone_index]);
            const Vec4 transformed_position = multiply(skinning_matrix, source_position_h);

            blended_position.x += transformed_position.x * weight;
            blended_position.y += transformed_position.y * weight;
            blended_position.z += transformed_position.z * weight;
            blended_position.w += transformed_position.w * weight;
            accumulated_weight += weight;
        }

        if (accumulated_weight <= 0.0F)
        {
            result_mesh.positions[vertex_index] = source_position;
            continue;
        }

        const float normalization_scale = 1.0F / accumulated_weight;
        blended_position.x *= normalization_scale;
        blended_position.y *= normalization_scale;
        blended_position.z *= normalization_scale;
        blended_position.w *= normalization_scale;

        result_mesh.positions[vertex_index] = divide_by_w(blended_position);
    }

    return result_mesh;
}

} // namespace transformer
