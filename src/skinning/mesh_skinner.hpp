#pragma once

#include "core/profiler.hpp"
#include "core/types.hpp"

namespace transformer
{

class MeshSkinner
{
public:
    Mesh skin(const Mesh& source_mesh, const BoneWeightsData& bone_weights_data, const BonePoseData& bone_pose_data,
              Profiler& profiler) const;
};

} // namespace transformer
