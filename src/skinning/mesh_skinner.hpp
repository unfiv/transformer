#pragma once

#include "core/profiler.hpp"
#include "core/types.hpp"

namespace transformer
{

class MeshSkinner
{
public:
    Mesh skin(const Mesh& source_mesh, const SkinningData& skinning_data, const BonePoseData& pose_data,
              Profiler& profiler) const;
};

} // namespace transformer
