#include "app/app.hpp"
#include "core/types.hpp"
#include "io/json_readers.hpp"
#include "io/obj_io.hpp"
#include "skinning/mesh_skinner.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using transformer::AppInput;
using transformer::JsonBonePoseReader;
using transformer::JsonBoneWeightsReader;
using transformer::JsonStatsWriter;
using transformer::MeshSkinner;
using transformer::ObjMeshReader;
using transformer::ObjMeshWriter;
using transformer::SkinningApp;

static void print_help(const char* prog)
{
    std::cerr << "Usage: " << (prog ? prog : "transformer")
              << " --mesh <meshFile.obj> --bones-weights <boneWeightFile.json>"
                 " --inverse-bind-pose <inverseBindPoseFile.json>"
                 " --new-pose <newPoseFile.json> --output <resultFile.obj>"
                 " --stats <statsFile.json> [--bench <N>]\n";
    std::cerr << "Input format:\n"
                 "  - weights json: { \"vertices\": [ { \"bone_indices\": [0,1,...], \"weights\": [..] }, ... ] }\n"
                 "  - pose json: { \"bones\": [ { \"matrix\": [16 column-major float values] }, ... ] }\n"
                 "\n"
                 "Options:\n"
                 "  --bench <N>  Run cpu_skinning N times in a loop and write summary stats.\n"
                 "               If omitted, skinning runs once.\n";
}

static bool parse_positive_int(const std::string& value, std::size_t& out)
{
    if (value.empty())
    {
        return false;
    }

    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(value.c_str(), &end, 10);
    if (end == nullptr || *end != '\0' || parsed == 0)
    {
        return false;
    }

    out = static_cast<std::size_t>(parsed);
    return true;
}

int main(int argc, char** argv)
{
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h"))
    {
        print_help(argc > 0 ? argv[0] : nullptr);
        return 0;
    }

    AppInput input;
    bool mesh_set = false;
    bool weights_set = false;
    bool inverse_bind_set = false;
    bool new_pose_set = false;
    bool output_set = false;
    bool stats_set = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        auto require_value = [&](const std::string& option) -> const char* {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for option: " << option << std::endl;
                return nullptr;
            }
            ++i;
            return argv[i];
        };

        if (arg == "--mesh")
        {
            const char* value = require_value(arg);
            if (value == nullptr) return 1;
            input.mesh_file = value;
            mesh_set = true;
        }
        else if (arg == "--bones-weights")
        {
            const char* value = require_value(arg);
            if (value == nullptr) return 1;
            input.weights_file = value;
            weights_set = true;
        }
        else if (arg == "--inverse-bind-pose")
        {
            const char* value = require_value(arg);
            if (value == nullptr) return 1;
            input.inverse_bind_pose_file = value;
            inverse_bind_set = true;
        }
        else if (arg == "--new-pose")
        {
            const char* value = require_value(arg);
            if (value == nullptr) return 1;
            input.new_pose_file = value;
            new_pose_set = true;
        }
        else if (arg == "--output")
        {
            const char* value = require_value(arg);
            if (value == nullptr) return 1;
            input.output_mesh_file = value;
            output_set = true;
        }
        else if (arg == "--stats")
        {
            const char* value = require_value(arg);
            if (value == nullptr) return 1;
            input.stats_file = value;
            stats_set = true;
        }
        else if (arg == "--bench")
        {
            const char* value = require_value(arg);
            if (value == nullptr) return 1;
            std::size_t bench_runs = 0;
            if (!parse_positive_int(value, bench_runs))
            {
                std::cerr << "Invalid value for --bench (expected positive integer): " << value << std::endl;
                return 1;
            }
            input.bench_runs = bench_runs;
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_help(argc > 0 ? argv[0] : nullptr);
            return 1;
        }
    }

    if (!(mesh_set && weights_set && inverse_bind_set && new_pose_set && output_set && stats_set))
    {
        std::cerr << "Missing required arguments." << std::endl;
        print_help(argc > 0 ? argv[0] : nullptr);
        return 1;
    }

    const ObjMeshReader mesh_reader;
    const JsonBoneWeightsReader bone_weights_reader;
    const JsonBonePoseReader pose_reader;
    const ObjMeshWriter mesh_writer;
    const JsonStatsWriter stats_writer;
    const MeshSkinner skinner;

    const SkinningApp app(mesh_reader, bone_weights_reader, pose_reader, mesh_writer, stats_writer, skinner);
    return app.run(input);
}
