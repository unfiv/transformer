#pragma once

#include "io/io_interfaces.hpp"

namespace transformer
{

class JsonWeightsReader : public IWeightsReader
{
public:
    SkinningData read(const std::string& weights_file, Profiler& profiler) const override;
};

class JsonBonePoseReader : public IBonePoseReader
{
public:
    std::vector<Mat4> read_matrices(const std::string& file_path, Profiler& profiler,
                                    const std::string& stage_name) const override;
};

class JsonStatsWriter : public IStatsWriter
{
public:
    void write(const std::string& output_file, const std::vector<TimingEntry>& timings) const override;
};

} // namespace transformer
