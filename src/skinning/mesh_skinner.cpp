#include "skinning/mesh_skinner.hpp"

#include "core/math_utils.hpp"
#include "core/profiler.hpp"

namespace transformer
{

Mesh MeshSkinner::skin(const Mesh& source_mesh, const BonePoseData& bone_pose_data, Profiler& profiler) const
{
    const auto scope = profiler.stage("cpu_skinning");

    Mesh result_mesh = source_mesh;

    for (std::size_t vertex_index = 0; vertex_index < source_mesh.vertex_count; ++vertex_index)
    {
        const Mesh::Entry& source_entry = source_mesh.entries[vertex_index];
        const Vec3& source_position = source_entry.vertex;
        const VertexBoneWeights& vertex_bone_weights = source_entry.bone_weights;

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
                multiply(bone_pose_data.bone_poses[bone_index][1], bone_pose_data.bone_poses[bone_index][0]);
            const Vec4 transformed_position = multiply(skinning_matrix, source_position_h);

            blended_position.x += transformed_position.x * weight;
            blended_position.y += transformed_position.y * weight;
            blended_position.z += transformed_position.z * weight;
            blended_position.w += transformed_position.w * weight;
            accumulated_weight += weight;
        }

        if (accumulated_weight <= 0.0F)
        {
            result_mesh.entries[vertex_index].vertex = source_position;
            continue;
        }

        const float normalization_scale = 1.0F / accumulated_weight;
        blended_position.x *= normalization_scale;
        blended_position.y *= normalization_scale;
        blended_position.z *= normalization_scale;
        blended_position.w *= normalization_scale;

        result_mesh.entries[vertex_index].vertex = divide_by_w(blended_position);
    }

    return result_mesh;
}

} // namespace transformer
