// Lost World Native LG: the quick settings as silent programs. One program,
// copied by build.bat under each setting's name ("2 - Display - 4x3.exe",
// "Graphics - Ultra.exe"...): it does what its own name says, with no window
// (the .bat files they replace flashed a console). Only a problem is shown,
// in a message box. Takes effect the next time the game starts.
//
// What they do, as the .bat files did:
//   2 - Display - ... copies 16x9\ or 4x3\display.ini to display.ini and sets
//                    Settings > WIDESCREEN (save\settings.ini: aspect)
//   2 - Crosshairs - ... sets crosshair
//   3 - Graphics - ... sets the graphics settings of that level
//   4 - Text - ...   copies textures\text_jurassic or text_original into textures\text
// save\settings.ini keeps every other line as it was; a key that is not there is added.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>

namespace {

using Keys = std::vector<std::pair<const char*, const char*>>;

struct Preset {
    const wchar_t* name;          // the program's name, without .exe
    Keys keys;                    // save\settings.ini
    std::vector<std::pair<const wchar_t*, const wchar_t*>> copies;   // from, to (beside the program)
};

// The graphics levels: exactly what the game's own Options > DISPLAY > QUALITY
// sets for LOW / MEDIUM / HIGH / ULTRA (QUALITY_LEVELS in
// src/ui/frontend_settings.cpp), so the menu shows that level afterwards. The
// other options (weather, haze, flashlight...) are left as they are, as QUALITY
// leaves them
Keys graphics(const char* scale, const char* half, const char* rt, const char* on, const char* bloom,
              const char* aa, const char* msaa, const char* aniso, const char* smooth) {
    return {{"resolution_3d", scale}, {"half_rate", half},  {"ray_tracing", rt},     {"ao", on},
            {"shadows", on},          {"light_shafts", on}, {"soft_particles", "1"}, {"sky", *on == '1' ? "procedural" : "original"},
            {"surface_detail", on},   {"bloom", bloom},     {"aa", aa},              {"msaa", msaa},
            {"anisotropic", aniso},   {"filter_2d", smooth}};
}

const std::vector<Preset>& presets() {
    static const std::vector<Preset> list = {
        {L"2 - Display - 4x3", {{"aspect", "4:3"}}, {{L"4x3\\display.ini", L"display.ini"}}},
        {L"2 - Display - 16x9", {{"aspect", "16:9"}}, {{L"16x9\\display.ini", L"display.ini"}}},
        {L"2 - Crosshairs - On", {{"crosshair", "1"}}, {}},
        {L"2 - Crosshairs - Off", {{"crosshair", "0"}}, {}},
        {L"3 - Graphics - Low",
         graphics("75", "1", "off", "0", "low", "none", "2", "2", "pixels"),
         {}},
        {L"3 - Graphics - Medium",
         graphics("100", "1", "off", "1", "low", "msaa", "2", "4", "smooth"),
         {}},
        {L"3 - Graphics - High",
         graphics("100", "0", "shadows", "1", "medium", "msaa", "4", "8", "smooth"),
         {}},
        {L"3 - Graphics - Ultra",
         graphics("100", "0", "ao", "1", "medium", "msaa", "4", "8", "smooth"),
         {}},
        {L"4 - Text - Jurassic Park", {},
         {{L"textures\\text_jurassic\\atlas.png", L"textures\\text\\atlas.png"},
          {L"textures\\text_jurassic\\glyphs.txt", L"textures\\text\\glyphs.txt"}}},
        {L"4 - Text - Original", {},
         {{L"textures\\text_original\\atlas.png", L"textures\\text\\atlas.png"},
          {L"textures\\text_original\\glyphs.txt", L"textures\\text\\glyphs.txt"}}},
    };
    return list;
}

std::wstring g_dir;   // the program's folder (the game's), with a backslash

void fail(const std::wstring& what) {
    MessageBoxW(nullptr, what.c_str(), L"Lost World Native LG", MB_ICONERROR | MB_OK);
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// save\settings.ini with `keys` set: their lines replaced, the rest kept
bool set_keys(const Keys& keys) {
    if (keys.empty()) return true;
    CreateDirectoryW((g_dir + L"save").c_str(), nullptr);
    std::wstring ini = g_dir + L"save\\settings.ini", tmp = ini + L".tmp";
    std::vector<std::string> lines;
    if (FILE* f = _wfopen(ini.c_str(), L"rb")) {
        std::string all;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) all.append(buf, n);
        fclose(f);
        size_t at = 0;
        while (at < all.size()) {
            size_t e = all.find('\n', at);
            std::string line = all.substr(at, e == std::string::npos ? std::string::npos : e - at);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
            if (e == std::string::npos) break;
            at = e + 1;
        }
    }
    std::vector<bool> done(keys.size(), false);
    for (std::string& line : lines) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        for (size_t k = 0; k < keys.size(); k++)
            if (key == keys[k].first) {
                line = std::string(keys[k].first) + " = " + keys[k].second;
                done[k] = true;
            }
    }
    for (size_t k = 0; k < keys.size(); k++)
        if (!done[k]) lines.push_back(std::string(keys[k].first) + " = " + keys[k].second);
    FILE* f = _wfopen(tmp.c_str(), L"wb");
    if (!f) return false;
    for (const std::string& line : lines) fprintf(f, "%s\r\n", line.c_str());
    bool ok = fclose(f) == 0;
    return ok && MoveFileExW(tmp.c_str(), ini.c_str(), MOVEFILE_REPLACE_EXISTING);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring self(path, n);
    size_t slash = self.find_last_of(L"\\/");
    g_dir = self.substr(0, slash + 1);
    std::wstring name = self.substr(slash + 1);
    if (name.size() > 4 && _wcsicmp(name.c_str() + name.size() - 4, L".exe") == 0) name.resize(name.size() - 4);

    const Preset* preset = nullptr;
    for (const Preset& p : presets())
        if (_wcsicmp(p.name, name.c_str()) == 0) preset = &p;
    if (!preset) {
        fail(L"\"" + name + L".exe\" is not one of the quick settings (2 - Display, 2 - Crosshairs, 3 - Graphics, 4 - Text): "
             L"keep these programs' names as they came.");
        return 1;
    }
    for (auto& [from, to] : preset->copies) {
        std::wstring src = g_dir + from, dst = g_dir + to;
        if (GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES) {
            fail(std::wstring(from) + L" is missing: nothing was changed.");
            return 1;
        }
    }
    for (auto& [from, to] : preset->copies) {
        std::wstring dst = g_dir + to;
        size_t s = dst.find_last_of(L'\\');
        if (s != std::wstring::npos) CreateDirectoryW(dst.substr(0, s).c_str(), nullptr);
        if (!CopyFileW((g_dir + from).c_str(), dst.c_str(), FALSE)) {
            fail(L"Could not write " + std::wstring(to) + L" (is the game still open?)");
            return 1;
        }
    }
    if (!set_keys(preset->keys)) {
        fail(L"Could not write save\\settings.ini (is the game still open?)");
        return 1;
    }
    return 0;
}
