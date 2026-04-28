#include "KJ_Mouse.h"

static std::atomic<HHOOK> mouseHook{nullptr};

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    HHOOK hook = mouseHook.load(std::memory_order_relaxed);
    HWND target = targetGameHwnd.load(std::memory_order_relaxed);
    if (nCode < 0 || !hook || !target) return ::CallNextHookEx(hook, nCode, wParam, lParam);

    auto ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    if (!ms) return ::CallNextHookEx(hook, nCode, wParam, lParam);

    if ((ms->flags & LLMHF_INJECTED) || ms->dwExtraInfo == KEYJ_INJECT_TAG)
        return ::CallNextHookEx(hook, nCode, wParam, lParam);

    int keyCode = 0;
    bool isDown = false;
    switch (wParam) {
        case WM_LBUTTONDOWN: isDown = true;  keyCode = KJ_MOUSE_LEFT;   break;
        case WM_LBUTTONUP:                   keyCode = KJ_MOUSE_LEFT;   break;
        case WM_MBUTTONDOWN: isDown = true;  keyCode = KJ_MOUSE_MIDDLE; break;
        case WM_MBUTTONUP:                   keyCode = KJ_MOUSE_MIDDLE; break;
        case WM_RBUTTONDOWN: isDown = true;  keyCode = KJ_MOUSE_RIGHT;  break;
        case WM_RBUTTONUP:                   keyCode = KJ_MOUSE_RIGHT;  break;
        case WM_XBUTTONDOWN: isDown = true;
            keyCode = (HIWORD(ms->mouseData) == XBUTTON1) ? KJ_MOUSE_X1 : KJ_MOUSE_X2;
            break;
        case WM_XBUTTONUP:
            keyCode = (HIWORD(ms->mouseData) == XBUTTON1) ? KJ_MOUSE_X1 : KJ_MOUSE_X2;
            break;
        case WM_MOUSEWHEEL:
            keyCode = (GET_WHEEL_DELTA_WPARAM(ms->mouseData) > 0) ? KJ_MOUSE_WHEEL_UP : KJ_MOUSE_WHEEL_DOWN;
            break;
        default: return ::CallNextHookEx(hook, nCode, wParam, lParam);
    }

    if (!KJ_Controls_IsMapped(keyCode)) return ::CallNextHookEx(hook, nCode, wParam, lParam);

    if (keyCode == KJ_MOUSE_WHEEL_UP || keyCode == KJ_MOUSE_WHEEL_DOWN)
        KJ_Controls_NotifyWheelPulse(keyCode);
    else
        keyPhysicalDown[keyCode & 0xFF].store(isDown, std::memory_order_relaxed);

    HWND fg = kj_cachedForeground.load(std::memory_order_relaxed);
    if (fg == target)
        return 1;

    return ::CallNextHookEx(hook, nCode, wParam, lParam);
}

bool KJ_Mouse_Install(HINSTANCE hInstance) {
    if (mouseHook.load(std::memory_order_relaxed)) return true;
    HHOOK hook = ::SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);
    mouseHook.store(hook, std::memory_order_release);
    return hook != nullptr;
}

void KJ_Mouse_Uninstall() {
    HHOOK hook = mouseHook.exchange(nullptr, std::memory_order_acq_rel);
    if (hook)
        ::UnhookWindowsHookEx(hook);
}

static void KJ_SendMouseInput(DWORD flags, DWORD mouseData = 0, int deltaX = 0, int deltaY = 0) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = deltaX;
    input.mi.dy = deltaY;
    input.mi.mouseData = mouseData;
    input.mi.dwFlags = flags;
    input.mi.dwExtraInfo = KEYJ_INJECT_TAG;
    ::SendInput(1, &input, sizeof(INPUT));
}

// Sends one button transition (down or up) for the five physical mouse buttons.
static void KJ_Mouse_ButtonEvent(int button, bool release) {
    switch (button) {
        case KJ_MOUSE_LEFT:
            KJ_SendMouseInput(release ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN);
            return;
        case KJ_MOUSE_RIGHT:
            KJ_SendMouseInput(release ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN);
            return;
        case KJ_MOUSE_MIDDLE:
            KJ_SendMouseInput(release ? MOUSEEVENTF_MIDDLEUP : MOUSEEVENTF_MIDDLEDOWN);
            return;
        case KJ_MOUSE_X1:
            KJ_SendMouseInput(release ? MOUSEEVENTF_XUP : MOUSEEVENTF_XDOWN, XBUTTON1);
            return;
        case KJ_MOUSE_X2:
            KJ_SendMouseInput(release ? MOUSEEVENTF_XUP : MOUSEEVENTF_XDOWN, XBUTTON2);
            return;
        default:
            return;
    }
}

void KJ_Mouse_ButtonClick(int button) {
    KJ_Mouse_ButtonEvent(button, false);
    KJ_Mouse_ButtonEvent(button, true);
}

void KJ_Mouse_ButtonHold(int button) {
    KJ_Mouse_ButtonEvent(button, false);
}

void KJ_Mouse_ButtonRelease(int button) {
    KJ_Mouse_ButtonEvent(button, true);
}

void KJ_Mouse_Wheel(int notches) {
    KJ_SendMouseInput(MOUSEEVENTF_WHEEL, static_cast<DWORD>(WHEEL_DELTA * notches));
}

void KJ_Mouse_Move(int deltaX, int deltaY) {
    KJ_SendMouseInput(MOUSEEVENTF_MOVE, 0, deltaX, deltaY);
}