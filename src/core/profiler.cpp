#include "core/profiler.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace transformer
{

BenchSummary compute_bench_summary(const std::vector<double>& run_microseconds)
{
    BenchSummary summary;
    summary.runs = run_microseconds.size();
    if (run_microseconds.empty())
    {
        return summary;
    }

    summary.min_microseconds = *std::min_element(run_microseconds.begin(), run_microseconds.end());
    summary.max_microseconds = *std::max_element(run_microseconds.begin(), run_microseconds.end());

    const double sum = std::accumulate(run_microseconds.begin(), run_microseconds.end(), 0.0);
    summary.mean_microseconds = sum / static_cast<double>(run_microseconds.size());

    std::vector<double> sorted = run_microseconds;
    std::sort(sorted.begin(), sorted.end());
    const std::size_t n = sorted.size();
    if (n % 2 == 0)
    {
        summary.median_microseconds = (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;
    }
    else
    {
        summary.median_microseconds = sorted[n / 2];
    }

    double variance = 0.0;
    for (double value : run_microseconds)
    {
        const double delta = value - summary.mean_microseconds;
        variance += delta * delta;
    }
    variance /= static_cast<double>(run_microseconds.size());
    summary.stddev_microseconds = std::sqrt(variance);

    return summary;
}

Profiler::ScopedStage::ScopedStage(Profiler& profiler, std::string stage_name)
    : profiler_(profiler)
    , stage_name_(std::move(stage_name))
    , start_(std::chrono::steady_clock::now())
{
}

Profiler::ScopedStage::~ScopedStage()
{
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double, std::micro>(end - start_).count();
    profiler_.record(stage_name_, elapsed);
}

Profiler::ScopedStage Profiler::stage(const std::string& stage_name)
{
    return ScopedStage(*this, stage_name);
}

void Profiler::record(const std::string& stage_name, double microseconds)
{
    entries_.push_back(TimingEntry{ stage_name, microseconds });
}

const std::vector<TimingEntry>& Profiler::entries() const
{
    return entries_;
}

} // namespace transformer
