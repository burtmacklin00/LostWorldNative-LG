// Music for the menus: third_party/menu/music.mp3 (or .wav), not in the
// repository, looped while a menu is up over the backdrop (not over a paused
// game) and faded out when the game starts. Played by Windows (MCI), apart
// from the game's own sound, on a thread of its own: MCI's player (DirectShow)
// wants a single-threaded COM apartment, and the window thread has a
// multithreaded one (the picture loading and the sound output set it up)
#pragma once
#include <atomic>
#include <string>
#include <thread>

class MenuMusic {
public:
    ~MenuMusic() { close(); }
    bool open();    // false: there is no music file (the file is opened on the music thread)
    void close();
    // Every frame: whether the music should be heard, at what volume (0..10, the
    // game's), and the seconds since the last call
    void update(bool want, int volume, double dt);

private:
    void run(std::wstring path);
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<int> volume_{0};    // 0..1000 wanted (0: paused)
    float level_ = 0;               // the fade, 0..1
};
