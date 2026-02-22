#pragma once

#include "core/math_types.hpp"

#include <string>
#include <vector>

namespace transformer
{

class Profiler;
struct StatsReport;

class IMeshReader
{
public:
    virtual ~IMeshReader() = default;
    virtual Mesh read(const std::string& mesh_file, Profiler& profiler) const = 0;
};

class IBoneWeightsReader
{
public:
    virtual ~IBoneWeightsReader() = default;
    virtual BoneWeightsData read(const std::string& weights_file, Profiler& profiler) const = 0;
};

class IBonePoseReader
{
public:
    virtual ~IBonePoseReader() = default;
    virtual std::vector<Mat4> read_matrices(const std::string& file_path, Profiler& profiler,
                                            const std::string& stage_name) const = 0;
};

class IMeshWriter
{
public:
    virtual ~IMeshWriter() = default;
    virtual void write(const std::string& output_file, const Mesh& mesh, Profiler& profiler) const = 0;
};

class IStatsWriter
{
public:
    virtual ~IStatsWriter() = default;
    virtual void write(const std::string& output_file, const StatsReport& stats) const = 0;
};

} // namespace transformer
