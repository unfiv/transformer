#pragma once

#include "core/math_types.hpp"

namespace transformer
{

	class Profiler;

	class MeshSkinner
	{
	public:
		void skin(const Mesh& source_mesh, const BonePoseData& bone_pose_data, Profiler& profiler, Mesh& result_mesh)
				const;
	};

}  // namespace transformer
