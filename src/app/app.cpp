#include "app/app.hpp"

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
        const Mesh source_mesh = mesh_reader_.read(input.mesh_file, profiler);
        const BoneWeightsData bone_weights_data = bone_weights_reader_.read(input.weights_file, profiler);

        BonePoseData bone_pose_data;
        bone_pose_data.inverse_bind_pose =
            bone_pose_reader_.read_matrices(input.inverse_bind_pose_file, profiler, "read_inverse_bind_pose_json");
        bone_pose_data.new_pose = bone_pose_reader_.read_matrices(input.new_pose_file, profiler, "read_new_pose_json");

        std::vector<double> bench_runs_microseconds;
        bench_runs_microseconds.reserve(input.bench_runs);

        Mesh skinned_mesh;
        for (std::size_t run_index = 0; run_index < input.bench_runs; ++run_index)
        {
            skinned_mesh = mesh_skinner_.skin(source_mesh, bone_weights_data, bone_pose_data, profiler);

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
