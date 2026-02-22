#include "skinning/mesh_skinner.hpp"

#include "core/math_utils.hpp"
#include "core/profiler.hpp"

// Non cross-platform
#include <immintrin.h> // SIMD

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

        // SIMD: Accumulator for weighted positions: [X, Y, Z, 0.0]
        __m128 v_acc = _mm_setzero_ps();

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
            // And we remove this block in favor of SIMD
            //const float tx = sm.m[0] * sp.x + sm.m[4] * sp.y + sm.m[8] * sp.z  + sm.m[12];
            //const float ty = sm.m[1] * sp.x + sm.m[5] * sp.y + sm.m[9] * sp.z  + sm.m[13];
            //const float tz = sm.m[2] * sp.x + sm.m[6] * sp.y + sm.m[10] * sp.z + sm.m[14];
            //blended_position_x += tx * weight;
            //blended_position_y += ty * weight;
            //blended_position_z += tz * weight;

            // SIMD:
            // Load columns of the matrix
            // Columns represent the transformed basis vectors and translation
            __m128 col0 = _mm_loadu_ps(&sm.m[0]);  // Basis X
            __m128 col1 = _mm_loadu_ps(&sm.m[4]);  // Basis Y
            __m128 col2 = _mm_loadu_ps(&sm.m[8]);  // Basis Z
            __m128 col3 = _mm_loadu_ps(&sm.m[12]); // Translation

            // Replicate (splat) coordinates to all 4 SIMD lanes
            __m128 xxxx = _mm_set1_ps(sp.x);
            __m128 yyyy = _mm_set1_ps(sp.y);
            __m128 zzzz = _mm_set1_ps(sp.z);

            // Matrix * Vector (Point) multiplication:
            // Result = Col0*X + Col1*Y + Col2*Z + Col3 (since W=1.0)
            __m128 res = _mm_mul_ps(col0, xxxx);
            res = _mm_add_ps(res, _mm_mul_ps(col1, yyyy));
            res = _mm_add_ps(res, _mm_mul_ps(col2, zzzz));
            res = _mm_add_ps(res, col3);

            // Multiply the result by bone weight and add to accumulator
            __m128 v_weight = _mm_set1_ps(weight);
            v_acc = _mm_add_ps(v_acc, _mm_mul_ps(res, v_weight));

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
        // And again remove this code in favor of SIMD result
        //result_mesh.entries[vertex_index].vertex = {blended_position_x, blended_position_y, blended_position_z};

        // SIMD
        // Write back to mesh (extracting X, Y, Z)
        alignas(16) float final_pos[4];
        _mm_store_ps(final_pos, v_acc);
        result_mesh.entries[vertex_index].vertex = { final_pos[0], final_pos[1], final_pos[2] };
    }
}

} // namespace transformer
