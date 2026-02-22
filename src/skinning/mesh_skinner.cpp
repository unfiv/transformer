#include "skinning/mesh_skinner.hpp"

#include "core/math_utils.hpp"
#include "core/profiler.hpp"

namespace transformer
{

void MeshSkinner::skin(const Mesh& source_mesh, const BonePoseData& bone_pose_data, Profiler& profiler, Mesh& result_mesh) const
{
    const auto scope = profiler.stage("cpu_skinning");

    // 1. Precompute skinning matrices for each bone to avoid redundant multiplications
    // 2. Another trick is is to use last matrix as empty one (255) with identity values to avoid branching
    auto skinning_matrices = std::make_unique<std::array<Mat4, 256>>();
    std::array<Mat4, 256>& precomputed_skinning_matrixes = *skinning_matrices;
    for (std::size_t bone_index = 0; bone_index < bone_pose_data.bone_poses.size(); ++bone_index)
    {
        precomputed_skinning_matrixes[bone_index] = multiply(bone_pose_data.bone_poses[bone_index][1], bone_pose_data.bone_poses[bone_index][0]);
    }

    for (std::size_t vertex_index = 0; vertex_index < source_mesh.vertex_count; ++vertex_index)
    {
        const Mesh::Entry& source_entry = source_mesh.entries[vertex_index];
        const Vec3& sp = source_entry.vertex;
        const VertexBoneWeights& vertex_bone_weights = source_entry.bone_weights;

        // Instead of Vec4 we can use 3 separate floats as we ignore w bevause of being sure sum of weights is 1
        float blended_position_x = 0.0F;
        float blended_position_y = 0.0F;
        float blended_position_z = 0.0F;

        // As soon as we sure weights' sum is always 1 we better skip branching 
        // and run over all 4 bones to make zero vector to add to blended one
        for (std::size_t i = 0; i < 4; ++i)
        {
            // We prepared data to be -1 in case of unused bone slot, so we can just cast it to uint8_t to get 255 index for zero matrix
            const auto safe_bone_index = static_cast<std::uint8_t>(vertex_bone_weights.bone_indices[i]);
            const float weight = vertex_bone_weights.weights[i];

            // We remove this branching because of being sure our weights' sum is always 1
            //if (bone_index < 0 || weight <= 0.0F)
            //{
            //    continue;
            //}

            // We can ignore full Ve4
            const Mat4& sm = precomputed_skinning_matrixes[safe_bone_index];

            // We can ignore full 4 dimension multiplication now
            //const Vec4 transformed_position = multiply(skinning_matrix, source_position_h);
            //blended_position.x += transformed_position.x * weight;
            //blended_position.y += transformed_position.y * weight;
            //blended_position.z += transformed_position.z * weight;
            //blended_position.w += transformed_position.w * weight;

            // In favor of lightier option without W
            const float tx = sm.m[0] * sp.x + sm.m[4] * sp.y + sm.m[8] * sp.z  + sm.m[12];
            const float ty = sm.m[1] * sp.x + sm.m[5] * sp.y + sm.m[9] * sp.z  + sm.m[13];
            const float tz = sm.m[2] * sp.x + sm.m[6] * sp.y + sm.m[10] * sp.z + sm.m[14];

            blended_position_x += tx * weight;
            blended_position_y += ty * weight;
            blended_position_z += tz * weight;

            // no more accumulated weight
            //accumulated_weight += weight;
        }

        // No more accumulated weight
        //if (accumulated_weight <= 0.0F)
        //{
        //    result_mesh.entries[vertex_index].vertex = source_position;
        //    continue;
        //}

        // Even no more normalization because of this weights' sum 1
        //const float normalization_scale = 1.0F / accumulated_weight;
        //blended_position.x *= normalization_scale;
        //blended_position.y *= normalization_scale;
        //blended_position.z *= normalization_scale;
        //blended_position.w *= normalization_scale;

        // Instead just a simple adding (assume some of the positions can be zero because of zero weights, but it doesn't matter for us)

        // Remove the division operation as well
        //result_mesh.entries[vertex_index].vertex = divide_by_w(blended_position);
        result_mesh.entries[vertex_index].vertex = {blended_position_x, blended_position_y, blended_position_z};
    }
}

} // namespace transformer
