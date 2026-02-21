#include "core/profiler.hpp"

#include <utility>

namespace transformer
{

Profiler::ScopedStage::ScopedStage(Profiler& profiler, std::string stage_name)
    : profiler_(profiler)
    , stage_name_(std::move(stage_name))
    , start_(std::chrono::steady_clock::now())
{
}

Profiler::ScopedStage::~ScopedStage()
{
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double, std::milli>(end - start_).count();
    profiler_.record(stage_name_, elapsed);
}

Profiler::ScopedStage Profiler::stage(const std::string& stage_name)
{
    return ScopedStage(*this, stage_name);
}

void Profiler::record(const std::string& stage_name, double milliseconds)
{
    entries_.push_back(TimingEntry{ stage_name, milliseconds });
}

const std::vector<TimingEntry>& Profiler::entries() const
{
    return entries_;
}

} // namespace transformer
