#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace transformer
{

struct TimingEntry
{
    std::string stage;
    double milliseconds = 0.0;
};

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
    void record(const std::string& stage_name, double milliseconds);
    [[nodiscard]] const std::vector<TimingEntry>& entries() const;

private:
    std::vector<TimingEntry> entries_;
};

} // namespace transformer
