#include "io/obj_io.hpp"

#include "core/profiler.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace transformer
{

	namespace
	{

		std::int16_t parse_face_index(const std::string& token)
		{
			// Supports forms: v, v/t, v//n, v/t/n
			const std::size_t slash_pos = token.find('/');
			const std::string vertex_str = (slash_pos == std::string::npos) ? token : token.substr(0, slash_pos);
			const int index_1_based = std::stoi(vertex_str);
			return static_cast<std::int16_t>(index_1_based - 1);
		}

	}  // namespace

	Mesh ObjMeshReader::read(const std::string& mesh_file, Profiler& profiler) const
	{
		const auto scope = profiler.stage("read_obj_mesh");

		std::ifstream input(mesh_file);
		if (!input)
		{
			throw std::runtime_error("Failed to open OBJ file: " + mesh_file);
		}

		Mesh mesh;
		std::string line;

		while (std::getline(input, line))
		{
			if (line.empty() || line[0] == '#')
			{
				continue;
			}

			std::istringstream iss(line);
			std::string prefix;
			iss >> prefix;

			if (prefix == "v")
			{
				Vec3 pos{};
				iss >> pos.x >> pos.y >> pos.z;
				mesh.entries.push_back({.vertex = pos});
				++mesh.vertex_count;
			}
			else if (prefix == "f")
			{
				std::string t0;
				std::string t1;
				std::string t2;
				iss >> t0 >> t1 >> t2;
				if (t0.empty() || t1.empty() || t2.empty())
				{
					throw std::runtime_error("Only triangulated OBJ faces are supported");
				}

				mesh.indices.push_back(parse_face_index(t0));
				mesh.indices.push_back(parse_face_index(t1));
				mesh.indices.push_back(parse_face_index(t2));
			}
		}

		return mesh;
	}

	void ObjMeshWriter::write(const std::string& output_file, const Mesh& mesh, Profiler& profiler) const
	{
		const auto scope = profiler.stage("write_obj_mesh");

		std::ofstream output(output_file);
		if (!output)
		{
			throw std::runtime_error("Failed to open output OBJ file: " + output_file);
		}

		output << "# Skinned mesh\n";

		for (std::size_t i = 0; i < mesh.vertex_count; ++i)
		{
			const Vec3& pos = mesh.entries[i].vertex;
			output << "v " << pos.x << ' ' << pos.y << ' ' << pos.z << '\n';
		}

		for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
		{
			output << "f " << mesh.indices[i] + 1 << ' ' << mesh.indices[i + 1] + 1 << ' ' << mesh.indices[i + 2] + 1
				   << '\n';
		}
	}

}  // namespace transformer
