#include "raw_mice.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace raw_mice {
namespace {

struct Mouse {
    HANDLE device;
    LONG x = 0, y = 0;        // screen pixels: the mouse's own pointer
    bool have_pos = false;
    bool left = false, right = false;
    bool clicked = false;
};

std::vector<Mouse> g_mice;
int g_owner[2] = {-1, -1};    // index in g_mice of each player's mouse
int g_last_clicked = -1;      // player 1's mouse until a second one joins
bool g_join_allowed = false;
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
    if (b & RI_MOUSE_LEFT_BUTTON_DOWN) m.left = m.clicked = true;
    if (b & RI_MOUSE_LEFT_BUTTON_UP) m.left = false;
    if (b & RI_MOUSE_RIGHT_BUTTON_DOWN) m.right = true;
    if (b & RI_MOUSE_RIGHT_BUTTON_UP) m.right = false;
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

bool two_mice() { return g_owner[1] >= 0; }

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
