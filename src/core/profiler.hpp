#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace transformer
{

struct TimingEntry
{
    std::string stage;
    double microseconds = 0.0;
};

struct BenchSummary
{
    std::size_t runs = 0;
    double min_microseconds = 0.0;
    double max_microseconds = 0.0;
    double mean_microseconds = 0.0;
    double median_microseconds = 0.0;
    double stddev_microseconds = 0.0;
};

struct StatsReport
{
    std::vector<TimingEntry> stages;
    std::optional<BenchSummary> bench_summary;
};


[[nodiscard]] BenchSummary compute_bench_summary(const std::vector<double>& run_microseconds);

class Profiler
{
public:
    class ScopedStage
    {
    public:
        ScopedStage(Profiler& profiler, std::string stage_name);
        ~ScopedStage();

    private:
        Profiler& profiler_;
        std::string stage_name_;
        std::chrono::steady_clock::time_point start_;
    };

    [[nodiscard]] ScopedStage stage(const std::string& stage_name);
    void record(const std::string& stage_name, double microseconds);
    [[nodiscard]] const std::vector<TimingEntry>& entries() const;

private:
    std::vector<TimingEntry> entries_;
};

} // namespace transformer
