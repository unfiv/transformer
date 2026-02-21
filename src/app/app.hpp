#pragma once

#include "io/io_interfaces.hpp"
#include "skinning/mesh_skinner.hpp"
#include "core/types.hpp"

namespace transformer
{

class SkinningApp
{
public:
    SkinningApp(const IMeshReader& mesh_reader, const IWeightsReader& weights_reader,
                const IBonePoseReader& pose_reader, const IMeshWriter& mesh_writer,
                const IStatsWriter& stats_writer, const MeshSkinner& skinner);

    int run(const AppInput& input) const;

private:
    const IMeshReader& mesh_reader_;
    const IWeightsReader& weights_reader_;
    const IBonePoseReader& pose_reader_;
    const IMeshWriter& mesh_writer_;
    const IStatsWriter& stats_writer_;
    const MeshSkinner& skinner_;
};

} // namespace transformer
