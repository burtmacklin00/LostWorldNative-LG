#include "crosshair_file.h"

#include <commdlg.h>

#include "../video/image_io.h"

std::string crosshair_path(int player) {
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string dir(path, n);
    dir = dir.substr(0, dir.find_last_of("\\/") + 1);
    return dir + (player ? "crosshair_p2.png" : "crosshair.png");
}

bool crosshair_load(int player, int& w, int& h, std::vector<uint8_t>& rgba) {
    return png_load(crosshair_path(player), w, h, rgba) || (player && png_load(crosshair_path(0), w, h, rgba));
}

bool crosshair_pick(int player) {
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = GetActiveWindow();   // the game's, so the dialog opens in front of it
    ofn.lpstrFilter = L"PNG pictures (*.png)\0*.png\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = player ? L"Player 2 crosshair" : L"Player 1 crosshair";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return false;
    // Through a copy beside the exe: the game finds it on the next launch too
    std::string to = crosshair_path(player);
    std::wstring wto(to.begin(), to.end());
    std::wstring tmp = wto + L".new";
    if (!CopyFileW(file, tmp.c_str(), FALSE)) return false;
    int w, h;
    std::vector<uint8_t> rgba;
    std::string tmp8(to + ".new");
    if (!png_load(tmp8, w, h, rgba) || !MoveFileExW(tmp.c_str(), wto.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}
