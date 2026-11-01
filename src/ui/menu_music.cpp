#include "menu_music.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace {

MCIERROR mci_error(const std::wstring& command) { return mciSendStringW(command.c_str(), nullptr, 0, nullptr); }
bool mci(const std::wstring& command) { return mci_error(command) == 0; }

// The file's full path with backslashes: MCI does not take every relative or
// forward-slash path
std::wstring full_path(const wchar_t* path) {
    wchar_t buf[MAX_PATH];
    DWORD n = GetFullPathNameW(path, MAX_PATH, buf, nullptr);
    std::wstring p = n > 0 && n < MAX_PATH ? std::wstring(buf, n) : std::wstring(path);
    std::replace(p.begin(), p.end(), L'/', L'\\');
    return p;
}

}  // namespace

bool MenuMusic::open() {
    if (thread_.joinable()) return true;
    for (const wchar_t* path : {L"third_party/menu/music.mp3", L"third_party/menu/music.wav"}) {
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) continue;
        stop_ = false;
        thread_ = std::thread(&MenuMusic::run, this, full_path(path));
        return true;
    }
    return false;
}

void MenuMusic::close() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
}

// The music thread: opens the file, then follows volume_ (0: paused where it
// is), with Windows messages handled meanwhile (DirectShow posts some)
void MenuMusic::run(std::wstring path) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    // mpegvideo plays MP3 and WAV alike and can loop ("repeat")
    MCIERROR err = mci_error(L"open \"" + path + L"\" type mpegvideo alias lw_menu_music");
    if (err) {
        wchar_t why[256] = L"";
        mciGetErrorStringW(err, why, 256);
        fprintf(stderr, "[menu] could not open the music %ls: %ls\n", path.c_str(), why);
        CoUninitialize();
        return;
    }
    printf("[menu] music %ls\n", path.c_str());
    bool playing = false;
    int sent = -1;
    while (!stop_) {
        int v = volume_;
        if (v > 0 && !playing) {
            playing = mci(L"play lw_menu_music repeat");
            sent = -1;
        } else if (v <= 0 && playing) {
            mci(L"pause lw_menu_music");
            playing = false;
        }
        if (playing && v != sent) {
            mci(L"setaudio lw_menu_music volume to " + std::to_wstring(v));
            sent = v;
        }
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 15, QS_ALLINPUT);
    }
    mci(L"close lw_menu_music");
    CoUninitialize();
}

void MenuMusic::update(bool want, int volume, double dt) {
    if (!thread_.joinable()) return;
    // A second to come in, half a second to go when the game starts
    level_ = want ? std::min(1.0f, level_ + float(dt) / 1.0f) : std::max(0.0f, level_ - float(dt) / 0.5f);
    // 0..1000; the game's volume 5 (the default) plays it at half
    int v = level_ <= 0 ? 0 : int(std::clamp(level_ * float(volume) / 10.0f, 0.0f, 1.0f) * 1000.0f + 0.5f);
    volume_ = std::max(v, level_ > 0 ? 1 : 0);
}
