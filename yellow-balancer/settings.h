#pragma once

#include <string>
#include <filesystem>
#include <boost/json.hpp>
#include "logger.h"
#include "encoding_string.h"

class Settings {
    int analysis_period_;
    std::vector<std::wstring> processes_;
    void CreateSettings(const std::filesystem::path& file_path);
public:
    bool Read(std::filesystem::path dir);
    int AnalysisPeriod() { return analysis_period_; }
    const std::vector<std::wstring>& Processes() const { return processes_; }
};
