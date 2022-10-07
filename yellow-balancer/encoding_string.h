#pragma once

#include <windows.h>
#include <string>

std::wstring Utf8ToWideChar(const std::string& str);
std::string WideCharToUtf8(const std::wstring& wstr);