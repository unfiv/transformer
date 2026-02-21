#include "app/app.hpp"
#include "io/json_readers.hpp"
#include "skinning/mesh_skinner.hpp"
#include "io/obj_io.hpp"
#include "core/types.hpp"

#include <iostream>
#include <string>

using transformer::AppInput;
using transformer::JsonBonePoseReader;
using transformer::JsonStatsWriter;
using transformer::JsonBoneWeightsReader;
using transformer::MeshSkinner;
using transformer::ObjMeshReader;
using transformer::ObjMeshWriter;
using transformer::SkinningApp;

static void print_help(const char* prog)
{
    std::cerr << "Usage: " << (prog ? prog : "transformer")
              << " <meshFile.obj> <boneWeightFile.json> <inverseBindPoseFile.json>"
                 " <newPoseFile.json> <resultFile.obj> <statsFile.json>\n";
    std::cerr << "Input format:\n"
                 "  - weights json: { \"vertices\": [ { \"bone_indices\": [0,1,...], \"weights\": [..] }, ... ] }\n"
                 "  - pose json: { \"bones\": [ { \"matrix\": [16 column-major float values] }, ... ] }\n";
}

int main(int argc, char** argv)
{
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h"))
    {
        print_help(argc > 0 ? argv[0] : nullptr);
        return 0;
    }

    if (argc != 7)
    {
        print_help(argc > 0 ? argv[0] : nullptr);
        return 1;
    }

    const AppInput input{
        .mesh_file = argv[1],
        .weights_file = argv[2],
        .inverse_bind_pose_file = argv[3],
        .new_pose_file = argv[4],
        .output_mesh_file = argv[5],
        .stats_file = argv[6],
    };

    const ObjMeshReader mesh_reader;
    const JsonBoneWeightsReader bone_weights_reader;
    const JsonBonePoseReader pose_reader;
    const ObjMeshWriter mesh_writer;
    const JsonStatsWriter stats_writer;
    const MeshSkinner skinner;

    const SkinningApp app(mesh_reader, bone_weights_reader, pose_reader, mesh_writer, stats_writer, skinner);
    return app.run(input);
}
