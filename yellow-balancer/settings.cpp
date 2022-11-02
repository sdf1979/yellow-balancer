#include "settings.h"

using namespace std;

namespace fs = std::filesystem;
namespace json = boost::json;

static auto LOGGER = Logger::getInstance();

void Settings::CreateSettings(const fs::path& file_path) {
    if (!fs::exists(file_path)) {
        const std::string json = R"({ 
  "switching_frequency_in_seconds" : 10,
  "cpu_analysis_period_in_seconds" : 60,
  "log_storage_duration_in_hours" : 24,
  "maximum_cpu_value" : 70,
  "delta_cpu_values" : 30,
  "processes" : ["rphost.exe"]
})";
        ofstream out(file_path);
        out << json;
        out.close();
        LOGGER->Print(string("Create file settings: ").append(file_path.string()), true);
    }
}

void ReadValue(json::object* j_object, int& value, const char* key, bool& result) {
    json::object::iterator it = j_object->find(key);
    if (it != j_object->cend()) {
        if (it->value().if_int64()) {
            value = static_cast<int>(it->value().as_int64());
        }
        else {
            result = false;
        }
    }
    else {
        result = false;
    }
}

void ReadValue(json::object* j_object, vector<wstring>& value, const char* key, bool& result) {
    json::object::iterator it = j_object->find(key);
    if (it != j_object->cend()) {
        if (it->value().if_array()) {
            json::array j_processes = it->value().as_array();
            for (auto it_process = j_processes.begin(); it_process < j_processes.end(); ++it_process) {
                if (it_process->if_string()) {
                    value.push_back(Utf8ToWideChar(it_process->as_string().c_str()));
                }
            }
        }
    }
    else {
        result = false;
    }
}

bool Settings::Read(fs::path dir) {
    fs::path file_path = dir.append(L"settings.json");
    std::string path_settings = file_path.string();
    if (!fs::exists(file_path)) {
        CreateSettings(file_path);
    }
    
    processes_.clear();
    bool is_correct = true;

    ifstream in(file_path);
    if (in.is_open()) {
        stringstream buffer;
        buffer << in.rdbuf();

        error_code ec;
        auto j_value = json::parse(buffer.str(), ec);
        if (ec) {
            LOGGER->Print(wstring(L"Parsing settings.json failed. ").append(Utf8ToWideChar(ec.message())), Logger::Type::Error, true);
            LOGGER->Print(L"Delete the settings.json file. Restart the program. The default settings will be created.", true);
            return false;
        }
        
        if (json::object* j_object = j_value.if_object()) {
            ReadValue(j_object, switching_frequency_, "switching_frequency_in_seconds", is_correct);
            ReadValue(j_object, cpu_analysis_period_, "cpu_analysis_period_in_seconds", is_correct);
            ReadValue(j_object, log_storage_duration_, "log_storage_duration_in_hours", is_correct);
            ReadValue(j_object, maximum_cpu_value_, "maximum_cpu_value", is_correct);
            ReadValue(j_object, delta_cpu_values_, "delta_cpu_values", is_correct);
            ReadValue(j_object, processes_, "processes", is_correct);
        }
        else {
            is_correct = false;
        }
        in.close();
    }
    else {
        LOGGER->Print(L"Can't open settings.json file.", Logger::Type::Error, true);
        is_correct = false;
    }

    if (processes_.empty()) {
        processes_.push_back(L"rphost.exe");
    }

    return is_correct;
 }