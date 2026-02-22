#pragma once

#include <array>
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
    std::vector<Vec3> positions;
    std::vector<std::int16_t> indices;
};

struct BoneWeightsData
{
    std::vector<VertexBoneWeights> per_vertex_weights;
};

struct BonePoseData
{
    std::vector<Mat4> inverse_bind_pose;
    std::vector<Mat4> new_pose;
};

} // namespace transformer
