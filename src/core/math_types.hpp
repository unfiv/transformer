#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace transformer
{

struct Vec3
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct Vec4
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 0.0F;
};

struct Mat4
{
    // Column-major, to match provided input data.
    std::array<float, 16> m{};
};

struct VertexBoneWeights
{
    std::array<std::int8_t, 4> bone_indices{ -1, -1, -1, -1 };
    std::array<float, 4> weights{ 0.0F, 0.0F, 0.0F, 0.0F };
};

struct Mesh
{
    struct Entry
    {
        Vec3 vertex{};
        VertexBoneWeights bone_weights{};
    };

    std::vector<Entry> entries;
    std::vector<std::int16_t> indices;
    std::size_t vertex_count = 0;
};

struct BoneWeightsData
{
    std::vector<VertexBoneWeights> per_vertex_weights;
};

struct BonePoseData
{
    // [bone_index][0 = inverse_bind_pose, 1 = new_pose]
    std::vector<std::array<Mat4, 2>> bone_poses;
};

} // namespace transformer
