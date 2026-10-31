// Where the running program is: files beside LostWorld.exe (the ROM zip,
// data\, the crosshair pictures, NVIDIA's DLL)
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

inline std::wstring exe_path_w() {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::wstring(path, n);
}

// Its folder with the separator at the end ("" if the path has none)
inline std::string exe_folder() {
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string s(path, n);
    return s.substr(0, s.find_last_of("\\/") + 1);
}

// Its folder without the separator ("." if the path has none)
inline std::wstring exe_folder_w() {
    std::wstring s = exe_path_w();
    size_t sep = s.find_last_of(L"\\/");
    return sep == std::wstring::npos ? L"." : s.substr(0, sep);
}
