#include "app/app.hpp"
#include "core/profiler.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace transformer
{

SkinningApp::SkinningApp(const IMeshReader& mesh_reader, const IBoneWeightsReader& bone_weights_reader,
                         const IBonePoseReader& bone_pose_reader, const IMeshWriter& mesh_writer,
                         const IStatsWriter& stats_writer, const MeshSkinner& mesh_skinner)
    : mesh_reader_(mesh_reader)
    , bone_weights_reader_(bone_weights_reader)
    , bone_pose_reader_(bone_pose_reader)
    , mesh_writer_(mesh_writer)
    , stats_writer_(stats_writer)
    , mesh_skinner_(mesh_skinner)
{
}

int SkinningApp::run(const AppInput& input) const
{
    Profiler profiler;
    const auto total_start = std::chrono::steady_clock::now();

    try
    {
        Mesh source_mesh = mesh_reader_.read(input.mesh_file, profiler);
        const BoneWeightsData bone_weights_data = bone_weights_reader_.read(input.weights_file, profiler);

        if (source_mesh.vertex_count != bone_weights_data.per_vertex_weights.size())
        {
            throw std::runtime_error("Vertex count mismatch between mesh and skinning weights");
        }

        for (std::size_t vertex_index = 0; vertex_index < source_mesh.vertex_count; ++vertex_index)
        {
            source_mesh.entries[vertex_index].bone_weights = bone_weights_data.per_vertex_weights[vertex_index];
        }

        BonePoseData bone_pose_data;
        const std::vector<Mat4> inverse_bind_pose =
            bone_pose_reader_.read_matrices(input.inverse_bind_pose_file, profiler, "read_inverse_bind_pose_json");
        const std::vector<Mat4> new_pose = bone_pose_reader_.read_matrices(input.new_pose_file, profiler, "read_new_pose_json");

        if (inverse_bind_pose.size() != new_pose.size())
        {
            throw std::runtime_error("Bone count mismatch between inverse bind pose and new pose");
        }

        bone_pose_data.bone_poses.reserve(inverse_bind_pose.size());
        for (std::size_t i = 0; i < inverse_bind_pose.size(); ++i)
        {
            bone_pose_data.bone_poses.push_back({ inverse_bind_pose[i], new_pose[i] });
        }

        std::vector<double> bench_runs_microseconds;
        bench_runs_microseconds.reserve(input.bench_runs);

        Mesh skinned_mesh = source_mesh;
        for (std::size_t run_index = 0; run_index < input.bench_runs; ++run_index)
        {
            mesh_skinner_.skin(source_mesh, bone_pose_data, profiler, skinned_mesh);

            if (input.bench_runs > 1)
            {
                const std::vector<TimingEntry>& entries = profiler.entries();
                if (entries.empty() || entries.back().stage != "cpu_skinning")
                {
                    throw std::runtime_error("Internal profiler error: missing cpu_skinning timing entry");
                }

                bench_runs_microseconds.push_back(entries.back().microseconds);
            }
        }

        mesh_writer_.write(input.output_mesh_file, skinned_mesh, profiler);

        const auto total_end = std::chrono::steady_clock::now();
        const auto total_us = std::chrono::duration<double, std::micro>(total_end - total_start).count();
        profiler.record("total", total_us);

        StatsReport report{ .stages = profiler.entries() };
        if (input.bench_runs > 1)
        {
            report.bench_summary = compute_bench_summary(bench_runs_microseconds);
        }

        stats_writer_.write(input.stats_file, report);

        std::cout << "Success" << std::endl;
        return 0;
    }
    catch (const std::exception& ex)
    {
        const auto total_end = std::chrono::steady_clock::now();
        const auto total_us = std::chrono::duration<double, std::micro>(total_end - total_start).count();
        profiler.record("total", total_us);

        try
        {
            const StatsReport report{ .stages = profiler.entries() };
            stats_writer_.write(input.stats_file, report);
        }
        catch (...)
        {
            // Best effort: do not mask original error.
        }

        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}

} // namespace transformer
