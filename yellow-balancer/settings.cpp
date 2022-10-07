#include "settings.h"

using namespace std;

namespace fs = std::filesystem;
namespace json = boost::json;

static auto LOGGER = Logger::getInstance();

void Settings::CreateSettings(const fs::path& file_path) {
    if (!fs::exists(file_path)) {
        const std::string json = R"({ 
  "analysis_period" : 20,
  "processes" : ["rphost.exe"]
})";
        ofstream out(file_path);
        out << json;
        out.close();
        LOGGER->Print(string("Create file settings: ").append(file_path.string()), true);
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
            
            json::object::iterator it = j_object->find("analysis_period");
            if (it != j_object->cend()) {
                if (it->value().if_int64()) {
                    analysis_period_ = it->value().as_int64();
                }
                else {
                    is_correct = false;
                }
            }
            else {
                is_correct = false;
            }

            it = j_object->find("processes");
            if (it != j_object->cend()) {
                if (it->value().if_array()) {
                    json::array j_processes = it->value().as_array();
                    for (auto it_process = j_processes.begin(); it_process < j_processes.end(); ++it_process) {
                        if (it_process->if_string()) {
                            processes_.push_back(Utf8ToWideChar(it_process->as_string().c_str()));
                        }
                    }
                }
            }
            else {
                is_correct = false;
            }
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