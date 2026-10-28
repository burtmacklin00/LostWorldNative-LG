#include "raw_mice.h"

#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <string>
#include <vector>

namespace raw_mice {
namespace {

struct Mouse {
    HANDLE device;
    LONG x = 0, y = 0;        // screen pixels: the mouse's own pointer
    bool have_pos = false;
    bool left = false, right = false;
    bool clicked = false;
    int left_presses = 0, right_presses = 0;   // for the menus (menu_gun)
    int fixed_player = -2;    // LightgunHID.ini: 0 / 1 that player's gun, -1 neither, -2 not looked up yet
};

// LightgunHID.ini beside LostWorld.exe (written by "1 - Light Gun Setup.exe"):
//   [Lightguns]
//   P1_Lightgun_HID = VID_1209&PID_0001
//   P2_Lightgun_HID = VID_1209&PID_0002
// pins each gun to its player from the start, whichever fires first. Other
// mice then move only the menu pointer. NoGuns = 1 (or no file) keeps the
// usual behaviour: the first mouse is player 1's, a second one joins as 2.
std::wstring g_gun_id[2];     // upper case, e.g. VID_1209&PID_0001; empty: not set
bool g_fixed = false;         // at least one gun is pinned

std::wstring upper(std::wstring s) {
    for (wchar_t& c : s) c = wchar_t(std::towupper(c));
    return s;
}

void load_gun_ids() {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring ini(path, n);
    ini = ini.substr(0, ini.find_last_of(L"\\/") + 1) + L"LightgunHID.ini";
    if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    if (GetPrivateProfileIntW(L"Lightguns", L"NoGuns", 0, ini.c_str())) {
        printf("[mouse] LightgunHID.ini: no light guns (mouse play)\n");
        return;
    }
    for (int p = 0; p < 2; p++) {
        wchar_t v[128] = {};
        GetPrivateProfileStringW(L"Lightguns", p ? L"P2_Lightgun_HID" : L"P1_Lightgun_HID", L"", v, 128, ini.c_str());
        std::wstring id = upper(v);
        while (!id.empty() && iswspace(id.back())) id.pop_back();
        while (!id.empty() && iswspace(id.front())) id.erase(id.begin());
        g_gun_id[p] = id;
        if (!id.empty()) {
            g_fixed = true;
            printf("[mouse] player %d gun: %ls\n", p + 1, id.c_str());
        }
    }
}

// Which player's gun a device is, from its name (\\?\HID#VID_1209&PID_0001&MI_02#...)
int gun_player(HANDLE device) {
    wchar_t name[512] = {};
    UINT size = 512;
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, name, &size) == UINT(-1)) return -1;
    std::wstring dev = upper(name);
    for (int p = 0; p < 2; p++)
        if (!g_gun_id[p].empty() && dev.find(g_gun_id[p]) != std::wstring::npos) return p;
    return -1;
}

std::vector<Mouse> g_mice;
int g_owner[2] = {-1, -1};    // index in g_mice of each player's mouse
int g_last_clicked = -1;      // player 1's mouse until a second one joins
bool g_join_allowed = false;
bool g_block_p2 = false;      // the menus are open: player 2's gun does not drive the pointer
bool g_last_from_p2 = false;  // the latest mouse event came from player 2's gun (while blocked)
bool g_p2_moved = false;      // ... and it moved the pointer since take_p2_moved()
RECT g_area{};                // the window's client area, screen pixels
bool g_have_area = false, g_clipped = false;

int find(HANDLE device) {
    for (size_t i = 0; i < g_mice.size(); i++)
        if (g_mice[i].device == device) return int(i);
    g_mice.push_back({device});
    return int(g_mice.size() - 1);
}

void keep_inside(Mouse& m) {
    if (!g_have_area) return;
    m.x = std::clamp(m.x, g_area.left, g_area.right - 1);
    m.y = std::clamp(m.y, g_area.top, g_area.bottom - 1);
}

}  // namespace

void init(HWND hwnd) {
    load_gun_ids();
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;   // generic desktop
    rid.usUsage = 0x02;       // mouse
    rid.hwndTarget = hwnd;
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) printf("[mouse] no raw input: a single mouse\n");
}

void on_input(HRAWINPUT input) {
    RAWINPUT ri;
    UINT size = sizeof(ri);
    if (GetRawInputData(input, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) == UINT(-1)) return;
    if (ri.header.dwType != RIM_TYPEMOUSE || !ri.header.hDevice) return;   // no device: input sent by software
    int i = find(ri.header.hDevice);
    Mouse& m = g_mice[i];
    const RAWMOUSE& rm = ri.data.mouse;
    if (rm.usFlags & MOUSE_MOVE_ABSOLUTE) {   // tablets and light guns: 0..65535 over the screen
        bool virt = rm.usFlags & MOUSE_VIRTUAL_DESKTOP;
        int left = virt ? GetSystemMetrics(SM_XVIRTUALSCREEN) : 0, top = virt ? GetSystemMetrics(SM_YVIRTUALSCREEN) : 0;
        int w = GetSystemMetrics(virt ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
        int h = GetSystemMetrics(virt ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
        m.x = left + MulDiv(rm.lLastX, w, 65535);
        m.y = top + MulDiv(rm.lLastY, h, 65535);
        m.have_pos = true;
    } else if (rm.lLastX || rm.lLastY) {   // one pixel per count, without the pointer's acceleration
        if (!m.have_pos) {
            POINT p;
            GetCursorPos(&p);
            m.x = p.x, m.y = p.y;
            m.have_pos = true;
        }
        m.x += rm.lLastX;
        m.y += rm.lLastY;
        keep_inside(m);
    }
    USHORT b = rm.usButtonFlags;
    // Player 2's gun in the menus: its events are marked so the window's
    // pointer messages that follow are ignored (raw input arrives first)
    if (g_fixed && m.fixed_player == -2) m.fixed_player = gun_player(ri.header.hDevice);
    int owner = g_fixed ? m.fixed_player : i == g_owner[1] ? 1 : i == g_owner[0] ? 0 : -1;
    g_last_from_p2 = g_block_p2 && owner == 1;
    if (g_last_from_p2 && ((rm.usFlags & MOUSE_MOVE_ABSOLUTE) || rm.lLastX || rm.lLastY)) g_p2_moved = true;
    if (b & RI_MOUSE_LEFT_BUTTON_DOWN) m.left = m.clicked = true, m.left_presses++;
    if (b & RI_MOUSE_RIGHT_BUTTON_DOWN) m.right_presses++;
    if (b & RI_MOUSE_LEFT_BUTTON_UP) m.left = false;
    if (b & RI_MOUSE_RIGHT_BUTTON_DOWN) m.right = true;
    if (b & RI_MOUSE_RIGHT_BUTTON_UP) m.right = false;
    if (g_fixed) {
        // Pinned guns: the device is its player's gun as soon as it reports
        // anything. A gun can show up as more than one mouse (interfaces):
        // the one used last is the one that counts
        if (m.fixed_player == -2) m.fixed_player = gun_player(ri.header.hDevice);
        if (m.fixed_player >= 0 && g_owner[m.fixed_player] != i) {
            if (g_owner[m.fixed_player] < 0) printf("[mouse] player %d gun found\n", m.fixed_player + 1);
            g_owner[m.fixed_player] = i;
        }
        return;
    }
    if (!(b & RI_MOUSE_LEFT_BUTTON_DOWN)) return;
    if (g_owner[1] < 0 && g_join_allowed && g_last_clicked >= 0 && i != g_last_clicked) {
        g_owner[0] = g_last_clicked;   // player 1 keeps the mouse it was playing with
        g_owner[1] = i;
        g_mice[g_owner[0]].have_pos = false;   // it starts where the pointer is
        printf("[mouse] second mouse: player 2's gun\n");
    }
    if (g_owner[1] < 0) g_last_clicked = i;
}

void update(HWND hwnd, bool join_allowed, bool in_game) {
    g_join_allowed = join_allowed;
    POINT tl{0, 0};
    RECT rc;
    GetClientRect(hwnd, &rc);
    ClientToScreen(hwnd, &tl);
    g_area = {tl.x, tl.y, tl.x + rc.right, tl.y + rc.bottom};
    g_have_area = rc.right > 0 && rc.bottom > 0;
    // With two mice the pointer moves with both: kept in the window so a
    // click never lands outside it (the menus get it back)
    bool clip = two_mice() && in_game && GetForegroundWindow() == hwnd && g_have_area;
    if (clip) ClipCursor(&g_area);
    else if (g_clipped) ClipCursor(nullptr);
    g_clipped = clip;
}

// With pinned guns each player has their own from the start (one that has not
// reported yet simply does nothing)
bool two_mice() { return g_fixed ? g_owner[0] >= 0 || g_owner[1] >= 0 : g_owner[1] >= 0; }

bool guns_pinned() { return g_fixed; }

MenuGun menu_gun(HWND hwnd) {
    MenuGun out{};
    for (int p = 1; p >= 0; p--) {   // what the guns did meanwhile is no shot in the game afterwards
        if (g_owner[p] < 0) continue;
        Mouse& m = g_mice[g_owner[p]];
        if (p == 0) {
            out.known = true;
            if (!m.have_pos) {
                POINT c;
                GetCursorPos(&c);
                m.x = c.x, m.y = c.y;
                m.have_pos = true;
            }
            out.pos = {m.x, m.y};
            ScreenToClient(hwnd, &out.pos);
            out.left_presses = m.left_presses, out.right_presses = m.right_presses;
        }
        m.left_presses = m.right_presses = 0;
        m.clicked = false;
    }
    return out;
}

void block_player2(bool block) {
    g_block_p2 = block;
    if (!block) g_last_from_p2 = g_p2_moved = false;
}
bool from_player2() { return g_last_from_p2; }
bool take_p2_moved() {
    bool moved = g_p2_moved;
    g_p2_moved = false;
    return moved;
}

void reset() {
    g_owner[0] = g_owner[1] = -1;
    if (g_clipped) ClipCursor(nullptr);
    g_clipped = false;
}

Gun gun(int player, HWND hwnd) {
    Gun g{};
    if (g_owner[player] < 0) return g;
    Mouse& m = g_mice[g_owner[player]];
    if (!m.have_pos) {
        POINT p;
        GetCursorPos(&p);
        m.x = p.x, m.y = p.y;
        m.have_pos = true;
    }
    g.pos = {m.x, m.y};
    ScreenToClient(hwnd, &g.pos);
    g.trigger = m.left;
    g.reload = m.right;
    g.clicked = m.clicked;
    m.clicked = false;
    return g;
}

}  // namespace raw_mice
