#include "KJ_Keyboard.h"

static std::atomic<HHOOK> keyboardHook{nullptr};

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    HHOOK hook = keyboardHook.load(std::memory_order_relaxed);
    HWND target = targetGameHwnd.load(std::memory_order_relaxed);
    if (nCode < 0 || !target)
        return ::CallNextHookEx(hook, nCode, wParam, lParam);

    auto kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    if (!kb) return ::CallNextHookEx(hook, nCode, wParam, lParam);

    if ((kb->flags & LLKHF_INJECTED) || kb->dwExtraInfo == KEYJ_INJECT_TAG)
        return ::CallNextHookEx(hook, nCode, wParam, lParam);

    int vk = static_cast<int>(kb->vkCode);
    bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

    bool wasAlreadyDown;
    if (isDown) {
        wasAlreadyDown = keyPhysicalDown[vk & 0xFF].exchange(true, std::memory_order_relaxed);
    } else {
        wasAlreadyDown = false;
        keyPhysicalDown[vk & 0xFF].store(false, std::memory_order_relaxed);
    }

    HWND fg = kj_cachedForeground.load(std::memory_order_relaxed);
    if (fg != target)
        return ::CallNextHookEx(hook, nCode, wParam, lParam);

    if (isDown && wasAlreadyDown)
        return 1;

    if (KJ_Controls_IsMapped(vk)) {
        // Allow Steams Shift+Tab & Alt+Tab window switching if Tab key is mapped.
        if (vk == KJ_KEY_TAB &&
            ((keyPhysicalDown[KJ_KEY_SHIFT_L & 0xFF].load(std::memory_order_relaxed) ||
              keyPhysicalDown[KJ_KEY_SHIFT_R & 0xFF].load(std::memory_order_relaxed)) ||
              keyPhysicalDown[KJ_KEY_ALT & 0xFF].load(std::memory_order_relaxed)))
        {
            return ::CallNextHookEx(hook, nCode, wParam, lParam);
        }
        return 1;
    }

    return ::CallNextHookEx(hook, nCode, wParam, lParam);
}

bool KJ_Keyboard_Install(HINSTANCE hInstance) {
    if (keyboardHook.load(std::memory_order_relaxed)) return true;
    HHOOK hook = ::SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    keyboardHook.store(hook, std::memory_order_release);
    return hook != nullptr;
}

void KJ_Keyboard_Uninstall() {
    HHOOK hook = keyboardHook.exchange(nullptr, std::memory_order_acq_rel);
    if (hook)
        ::UnhookWindowsHookEx(hook);
}

static void KJ_FillKeyInput(WORD vk, DWORD extraFlags, INPUT* out) {
    // MapVirtualKeyW(vk, 4) == MapVirtualKeyW(vk, MAPVK_VK_TO_VSC_EX);
    UINT scEx = MapVirtualKeyW(vk, 4);
    out->type           = INPUT_KEYBOARD;
    out->ki.wVk         = vk;
    out->ki.wScan       = static_cast<WORD>(scEx & 0xFF);
    out->ki.dwFlags     = extraFlags | ((scEx >> 8) ? KEYEVENTF_EXTENDEDKEY : 0);
    out->ki.dwExtraInfo = KEYJ_INJECT_TAG;
}

void KJ_Keyboard_KeyPress(WORD vk) {
    INPUT inputs[2] = {};
    KJ_FillKeyInput(vk, 0, &inputs[0]);
    KJ_FillKeyInput(vk, KEYEVENTF_KEYUP, &inputs[1]);
    ::SendInput(2, inputs, sizeof(INPUT));
}

void KJ_Keyboard_KeyHold(WORD vk) {
    INPUT input = {};
    KJ_FillKeyInput(vk, 0, &input);
    ::SendInput(1, &input, sizeof(INPUT));
}

void KJ_Keyboard_KeyRelease(WORD vk) {
    INPUT input = {};
    KJ_FillKeyInput(vk, KEYEVENTF_KEYUP, &input);
    ::SendInput(1, &input, sizeof(INPUT));
}