#pragma once

#include "core/app_input.hpp"
#include "core/math_types.hpp"
#include "io/io_interfaces.hpp"
#include "skinning/mesh_skinner.hpp"

namespace transformer
{

	class SkinningApp
	{
	public:
		SkinningApp(
				const IMeshReader& mesh_reader,
				const IBoneWeightsReader& bone_weights_reader,
				const IBonePoseReader& bone_pose_reader,
				const IMeshWriter& mesh_writer,
				const IStatsWriter& stats_writer,
				const MeshSkinner& mesh_skinner);

		int run(const AppInput& input) const;

	private:
		const IMeshReader& mesh_reader_;
		const IBoneWeightsReader& bone_weights_reader_;
		const IBonePoseReader& bone_pose_reader_;
		const IMeshWriter& mesh_writer_;
		const IStatsWriter& stats_writer_;
		const MeshSkinner& mesh_skinner_;
	};

}  // namespace transformer
