#pragma once

#include <cstddef>
#include <string>

namespace transformer
{

	struct AppInput
	{
		std::string mesh_file;
		std::string weights_file;
		std::string inverse_bind_pose_file;
		std::string new_pose_file;
		std::string output_mesh_file;
		std::string stats_file;
		std::size_t bench_runs = 1;
	};

}  // namespace transformer
