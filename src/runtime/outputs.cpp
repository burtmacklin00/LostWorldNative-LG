#include "outputs.h"

#define NOMINMAX   // windows.h (through winsock2.h) would make min / max macros
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "inputs.h"
#include "ppc_rt.h"

namespace outputs {
namespace {

constexpr const char* GAME = "jplostworldsa";
constexpr unsigned short PORT = 8000;

// Game RAM (docs/game_ram.md; the gun values found in fn_0000E4D4, the shot,
// fn_0000C050, the reload, fn_0000BD88, the standard gun, and fn_00042A60,
// the gun pickups). Each is one int per player (+4 for player 2)
constexpr uint32_t AMMO = 0x1A3680;        // bullets left
constexpr uint32_t UNLIMITED = 0x1A3698;   // 1: shots don't use bullets (pickup)
constexpr uint32_t GUN_DELAY = 0x1A36B4;   // frames between shots: 12 standard, 30 / 20 / 300 special
constexpr uint32_t GUN_AUTO = 0x1A3690;    // 7 standard, 25 / 15 / 1 special
constexpr uint32_t LIVES[2] = {0x1A3720, 0x1A377C};
constexpr uint32_t SHOTS = 0x1A6214;       // half word, +0x10 for player 2: shots fired this stage
constexpr uint32_t SLOT_STATE = 0x1A3791;  // +0x10 for player 2: 1 / 2 playing, 0 CONTINUE, 3 out

// ---- the server ---------------------------------------------------------------

std::mutex g_mutex;
std::condition_variable g_wake;
std::vector<SOCKET> g_clients;
std::vector<std::pair<SOCKET, std::string>> g_queue;   // what to send to whom
std::map<std::string, int> g_values;                    // every output's current value
SOCKET g_listen = INVALID_SOCKET;
std::thread g_accept, g_send;
std::atomic<bool> g_running{false};

void enqueue_all(const std::string& text) {   // g_mutex held
    for (SOCKET s : g_clients) g_queue.push_back({s, text});
}

void set(const std::string& name, int value) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_values.find(name);
    if (it != g_values.end() && it->second == value) return;
    g_values[name] = value;
    enqueue_all(name + " = " + std::to_string(value) + "\r");
    g_wake.notify_one();
}

void accept_loop() {
    while (g_running) {
        SOCKET s = accept(g_listen, nullptr, nullptr);
        if (s == INVALID_SOCKET) {
            if (!g_running) break;
            Sleep(100);
            continue;
        }
        int one = 1;
        setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof(one));   // recoil can't wait
        std::lock_guard<std::mutex> lock(g_mutex);
        g_clients.push_back(s);
        std::string hello = std::string("mame_start = ") + GAME + "\r";
        for (auto& [name, value] : g_values) hello += name + " = " + std::to_string(value) + "\r";
        g_queue.push_back({s, hello});
        g_wake.notify_one();
        printf("[outputs] a client connected (Hook of the Reaper?)\n");
    }
}

void send_loop() {
    std::unique_lock<std::mutex> lock(g_mutex);
    while (true) {
        g_wake.wait(lock, [] { return !g_queue.empty() || !g_running; });
        auto queue = std::move(g_queue);
        g_queue.clear();
        lock.unlock();
        std::vector<SOCKET> dead;
        for (auto& [s, text] : queue)
            if (send(s, text.data(), int(text.size()), 0) == SOCKET_ERROR) dead.push_back(s);
        lock.lock();
        for (SOCKET s : dead) {
            auto it = std::find(g_clients.begin(), g_clients.end(), s);
            if (it != g_clients.end()) {
                closesocket(s);
                g_clients.erase(it);
                printf("[outputs] a client left\n");
            }
        }
        if (!g_running && g_queue.empty()) break;
    }
}

// ---- the game's state -> signals ----------------------------------------------------

// The gun in hand, from the pickup's settings (fn_00042A60): 0 standard,
// 1 the 30-frame gun (item types 2 and 4), 2 the 20-frame gun (type 3),
// 3 the gun both players get (type 5)
int weapon_of(int delay, int autofire) {
    if (delay == 30 && autofire == 25) return 1;
    if (delay == 20 && autofire == 15) return 2;
    if (delay == 300) return 3;
    return 0;
}

// Frames the recoil signal stays at 1 for one bullet: 2 (~35 ms) for the
// standard gun, longer for the special guns, so a gun driven by Recoil_Value
// gives a heavier single kick; Recoil (0 -> 1) guns get one kick either way
constexpr int RECOIL_FRAMES[4] = {2, 4, 3, 5};

struct Player {
    int shots = -1;          // the shot counter last frame (-1: not read yet)
    int pending = 0;         // bullets fired whose recoil has not been sent yet
    int recoil_on = 0;       // frames left with the recoil signal at 1
    int recoil_off = 0;      // frames at 0 before the next one (so each is a new 0 -> 1)
    int lives = -1;
    int damaged = 0;         // frames left with P_Damaged at 1
};
Player g_player[2];

}  // namespace

void start() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;
    g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen == INVALID_SOCKET) return;
    int one = 1;
    setsockopt(g_listen, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // this PC only
    if (bind(g_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR ||
        listen(g_listen, 4) == SOCKET_ERROR) {
        printf("[outputs] port %d is in use: no outputs\n", PORT);
        closesocket(g_listen);
        g_listen = INVALID_SOCKET;
        return;
    }
    g_running = true;
    g_accept = std::thread(accept_loop);
    g_send = std::thread(send_loop);
    for (int p = 1; p <= 2; p++)
        for (const char* s : {"_CtmRecoil", "_Ammo", "_Life", "_Damaged", "_Weapon", "_Special", "_Unlimited", "_LmpStart"})
            set("P" + std::to_string(p) + s, 0);
    printf("[outputs] Hook of the Reaper outputs on port %d (%s)\n", PORT, GAME);
}

void stop() {
    if (!g_running) return;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        enqueue_all("mame_stop = 1\r");
        g_running = false;
        g_wake.notify_one();
    }
    if (g_send.joinable()) g_send.join();
    closesocket(g_listen);   // ends accept()
    if (g_accept.joinable()) g_accept.join();
    std::lock_guard<std::mutex> lock(g_mutex);
    for (SOCKET s : g_clients) closesocket(s);
    g_clients.clear();
    WSACleanup();
}

void frame(unsigned long long game_frame) {
    if (!g_running) return;
    for (int p = 0; p < 2; p++) {
        Player& pl = g_player[p];
        std::string P = "P" + std::to_string(p + 1);
        uint8_t state = rd8(SLOT_STATE + 0x10 * p);
        bool playing = state == 1 || state == 2;
        int lives = int(rd32(LIVES[p]));
        int shots = rd16(SHOTS + 0x10 * p);
        int delay = int(rd32(GUN_DELAY + 4 * p)), autofire = int(rd32(GUN_AUTO + 4 * p));
        int weapon = playing ? weapon_of(delay, autofire) : 0;

        // Recoil: one per bullet the game counted. The counter restarts at 0
        // each stage (a drop is no shot); outside play nothing kicks. A jump
        // of more than a few in one frame is not shooting either: a saved
        // game state was loaded (the boss fights, survival) with its own count
        constexpr int MAX_SHOTS_PER_FRAME = 4;   // the fastest gun fires once in 12 frames
        int fired = shots - pl.shots;
        if (pl.shots >= 0 && fired > 0 && fired <= MAX_SHOTS_PER_FRAME && playing) pl.pending += fired;
        pl.shots = shots;
        if (!playing) pl.pending = 0;
        if (pl.recoil_on > 0) {
            if (--pl.recoil_on == 0) {
                set(P + "_CtmRecoil", 0);
                pl.recoil_off = 1;
            }
        } else if (pl.recoil_off > 0) {
            pl.recoil_off--;
        } else if (pl.pending > 0) {
            pl.pending--;
            pl.recoil_on = RECOIL_FRAMES[weapon];
            set(P + "_CtmRecoil", 1);
        }

        // Lives, damage and death: a hit that leaves lives is damage; the one
        // that takes the last life is only the death (P_Life = 0), so the two
        // light effects never collide
        int shown = playing ? std::max(lives, 0) : 0;
        if (pl.lives > 0 && shown < pl.lives && shown > 0 && playing) pl.damaged = 6;
        pl.lives = shown;
        set(P + "_Life", shown);
        if (pl.damaged > 0) {
            set(P + "_Damaged", 1);
            if (--pl.damaged == 0) set(P + "_Damaged", 0);
        }

        set(P + "_Ammo", playing ? int(rd32(AMMO + 4 * p)) : 0);
        set(P + "_Weapon", weapon);
        set(P + "_Special", weapon != 0 ? 1 : 0);
        set(P + "_Unlimited", playing && rd32(UNLIMITED + 4 * p) != 0 ? 1 : 0);
        bool can_join = !(p == 1 && g_inputs.block_p2);   // survival: player 2 can't join
        set(P + "_LmpStart", can_join && !playing && (game_frame / 30) % 2 == 0 ? 1 : 0);   // ~1 blink a second
    }
}

}  // namespace outputs
