#pragma once

#include <string>
#include <filesystem>
#include <boost/json.hpp>
#include "logger.h"
#include "encoding_string.h"

class Settings {
    int switching_frequency_;
    int cpu_analysis_period_;
    int log_storage_duration_;
    int maximum_cpu_value_;
    int delta_cpu_values_;
    std::vector<std::wstring> processes_;
    void CreateSettings(const std::filesystem::path& file_path);
public:
    bool Read(std::filesystem::path dir);
    int SwitchingFrequency() { return switching_frequency_; }
    int CpuAnalysisPeriod() { return cpu_analysis_period_; }
    int LogStorageDuration() { return log_storage_duration_; }
    int MaximumCpuValue() { return maximum_cpu_value_; }
    int DeltaCpuValues() { return delta_cpu_values_; }
    const std::vector<std::wstring>& Processes() const { return processes_; }
};
