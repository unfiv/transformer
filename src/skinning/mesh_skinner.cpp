#include "skinning/mesh_skinner.hpp"

#include "core/math_utils.hpp"

#include <stdexcept>

namespace transformer
{

Mesh MeshSkinner::skin(const Mesh& source_mesh, const SkinningData& skinning_data, const BonePoseData& pose_data,
                       Profiler& profiler) const
{
    const auto scope = profiler.stage("cpu_skinning");

    if (source_mesh.positions.size() != skinning_data.influences.size())
    {
        throw std::runtime_error("Vertex count mismatch between mesh and skinning weights");
    }

    if (pose_data.inverse_bind_pose.size() != pose_data.new_pose.size())
    {
        throw std::runtime_error("Bone count mismatch between inverse bind pose and new pose");
    }

    Mesh result = source_mesh;

    for (std::size_t vertex_index = 0; vertex_index < source_mesh.positions.size(); ++vertex_index)
    {
        const Vec3& source_position = source_mesh.positions[vertex_index];
        const VertexInfluence& influence = skinning_data.influences[vertex_index];

        Vec4 blended{ 0.0F, 0.0F, 0.0F, 0.0F };
        const Vec4 source_homogeneous{ source_position.x, source_position.y, source_position.z, 1.0F };

        for (std::size_t i = 0; i < 4; ++i)
        {
            const std::int32_t bone_index = influence.bone_indices[i];
            const float weight = influence.weights[i];

            if (bone_index < 0 || weight <= 0.0F)
            {
                continue;
            }

            if (static_cast<std::size_t>(bone_index) >= pose_data.new_pose.size())
            {
                throw std::runtime_error("Bone index out of range in skinning data");
            }

            const Mat4 skin_matrix = multiply(pose_data.new_pose[bone_index], pose_data.inverse_bind_pose[bone_index]);
            const Vec4 transformed = multiply(skin_matrix, source_homogeneous);

            blended.x += transformed.x * weight;
            blended.y += transformed.y * weight;
            blended.z += transformed.z * weight;
            blended.w += transformed.w * weight;
        }

        result.positions[vertex_index] = divide_by_w(blended);
    }

    return result;
}

} // namespace transformer
