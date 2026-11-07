#include "KJ_Controls.h"
#include "KJ_Joystick.h"
#include "KJ_Global.h"
#include <mmsystem.h>
#include <xinput.h>
#include <mutex>
#include <cstdio>
#include <exception>

std::atomic<bool> KJ_InputThreadActive{false};
std::atomic<bool> keyPhysicalDown[256];

// Recursive so a mapped callback is allowed to call back into the API.
static std::recursive_mutex mutex;

struct KeyState {
    bool isDown = false;
    bool wasDown = false;
    void(*onPress)() = nullptr;
    void(*onHeld)() = nullptr;
    void(*onRelease)() = nullptr;
};

static KeyState keyStates[KJ_KEY_ARRAY_SIZE];
static std::atomic<bool> keyMapped[KJ_KEY_ARRAY_SIZE];

// Mouse
static void(*mouseMoveCb)(int, int) = nullptr;
static std::atomic<int> mouseDeltaX{0};
static std::atomic<int> mouseDeltaY{0};
static POINT lastCursorPos = {};
static std::atomic<int> wheelPulseUp{0};    // KJ_MOUSE_WHEEL_UP
static std::atomic<int> wheelPulseDown{0};  // KJ_MOUSE_WHEEL_DOWN

// Controllers
static bool xInputConnected = false;
static DWORD xInputRetryTick = 0;
static bool joystickConnected = false;
static DWORD joystickRetryTick = 0;

static inline int KeyIndex(int key) { return key + KJ_KEY_OFFSET; }
static inline bool ValidIndex(int idx) { return idx >= 0 && idx < KJ_KEY_ARRAY_SIZE; }

// Dispatch_Events() runs on the polling thread. An exception escaping a thread
// entry point calls std::terminate() -> abort(), which surfaces as a Visual C++
// runtime error. Log the error instead of crashing, and fix the offending callback.
static KJ_CallbackErrorHandler callbackErrorHandler = nullptr;

void KJ_Controls_SetCallbackErrorHandler(KJ_CallbackErrorHandler handler)
{
    callbackErrorHandler = handler;
}

static void Report_Callback_Error(const char* callbackName, int key, const char* message)
{
    if (callbackErrorHandler)
    {
        callbackErrorHandler(callbackName, key, message ? message : "unknown exception");
        return;
    }
    std::fprintf(stderr, "[Key_Jacker] exception in %s callback for key %d: %s\n",
                 callbackName, key, message ? message : "unknown exception");
    std::fflush(stderr);
}

static void Invoke_Callback(void (*callback)(), const char* callbackName, int key)
{
    if (!callback) return;
    try
    {
        callback();
    }
    catch (const std::exception& e)
    {
        Report_Callback_Error(callbackName, key, e.what());
    }
    catch (...)
    {
        Report_Callback_Error(callbackName, key, "unknown exception");
    }
}

static void Invoke_Mouse_Move_Callback(void (*callback)(int, int), int dx, int dy)
{
    if (!callback) return;
    try
    {
        callback(dx, dy);
    }
    catch (const std::exception& e)
    {
        Report_Callback_Error("mouseMove", 0, e.what());
    }
    catch (...)
    {
        Report_Callback_Error("mouseMove", 0, "unknown exception");
    }
}

bool KJ_Controls_IsMapped(int key)
{
    int idx = KeyIndex(key);
    return ValidIndex(idx) && keyMapped[idx].load(std::memory_order_relaxed);
}

void KJ_ClearAllPhysicalDown()
{
    for (int i = 0; i < 256; ++i)
        keyPhysicalDown[i].store(false, std::memory_order_relaxed);
}

static void ClearXboxStates()
{
    for (int key = KJ_XBOX_A; key >= KJ_XBOX_RS_RIGHT; --key) {
        int idx = KeyIndex(key);
        if (ValidIndex(idx))
            keyStates[idx].isDown = false;
    }
}

/**********************************************************Polling**********************************************************/

static void PollKeyboard()
{
    for (int vk = 1; vk < 256; ++vk)
        keyStates[KeyIndex(vk)].isDown = keyPhysicalDown[vk].load(std::memory_order_relaxed);
}

static void PollMouse()
{
    POINT cur;
    if (!GetCursorPos(&cur)) return;

    int dx = cur.x - lastCursorPos.x;
    int dy = cur.y - lastCursorPos.y;

    if (dx != 0 || dy != 0) {
        mouseDeltaX.store(dx, std::memory_order_relaxed);
        mouseDeltaY.store(dy, std::memory_order_relaxed);
    }
}

static void PollWheel()
{
    int idxUp = KeyIndex(KJ_MOUSE_WHEEL_UP);
    int idxDn = KeyIndex(KJ_MOUSE_WHEEL_DOWN);
    if (ValidIndex(idxUp))
        keyStates[idxUp].isDown = wheelPulseUp.exchange(0, std::memory_order_relaxed) > 0;
    if (ValidIndex(idxDn))
        keyStates[idxDn].isDown = wheelPulseDown.exchange(0, std::memory_order_relaxed) > 0;
}

void KJ_Controls_NotifyWheelPulse(int key)
{
    if (key == KJ_MOUSE_WHEEL_UP)
        wheelPulseUp.fetch_add(1, std::memory_order_relaxed);
    else if (key == KJ_MOUSE_WHEEL_DOWN)
        wheelPulseDown.fetch_add(1, std::memory_order_relaxed);
}

static const struct { int key; WORD bit; } s_xboxButtons[] = {
    { KJ_XBOX_A,          XINPUT_GAMEPAD_A              },
    { KJ_XBOX_B,          XINPUT_GAMEPAD_B              },
    { KJ_XBOX_X,          XINPUT_GAMEPAD_X              },
    { KJ_XBOX_Y,          XINPUT_GAMEPAD_Y              },
    { KJ_XBOX_LB,         XINPUT_GAMEPAD_LEFT_SHOULDER  },
    { KJ_XBOX_RB,         XINPUT_GAMEPAD_RIGHT_SHOULDER },
    { KJ_XBOX_LS_CLICK,   XINPUT_GAMEPAD_LEFT_THUMB     },
    { KJ_XBOX_RS_CLICK,   XINPUT_GAMEPAD_RIGHT_THUMB    },
    { KJ_XBOX_BACK,       XINPUT_GAMEPAD_BACK           },
    { KJ_XBOX_START,      XINPUT_GAMEPAD_START          },
    { KJ_XBOX_DPAD_UP,    XINPUT_GAMEPAD_DPAD_UP        },
    { KJ_XBOX_DPAD_DOWN,  XINPUT_GAMEPAD_DPAD_DOWN      },
    { KJ_XBOX_DPAD_LEFT,  XINPUT_GAMEPAD_DPAD_LEFT      },
    { KJ_XBOX_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT     },
};

static void PollXboxController()
{
    if (!xInputConnected) {
        DWORD now = GetTickCount();
        if (now - xInputRetryTick < 1000) return;
        xInputRetryTick = now;
    }

    XINPUT_STATE xState = {};
    if (XInputGetState(0, &xState) != ERROR_SUCCESS) {
        if (xInputConnected) {
            xInputConnected = false;
            ClearXboxStates();
        }
        return;
    }
    xInputConnected = true;

    const XINPUT_GAMEPAD& gp = xState.Gamepad;

    for (const auto& b : s_xboxButtons) {
        int idx = KeyIndex(b.key);
        if (ValidIndex(idx))
            keyStates[idx].isDown = (gp.wButtons & b.bit) != 0;
    }

    // Triggers
    auto setTrigger = [&](int key, BYTE val) {
        int idx = KeyIndex(key);
        if (ValidIndex(idx))
            keyStates[idx].isDown = val > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    };
    setTrigger(KJ_XBOX_LT, gp.bLeftTrigger);
    setTrigger(KJ_XBOX_RT, gp.bRightTrigger);

    // Thumbsticks with hysteresis
    auto setAxis = [&](int posKey, int negKey, SHORT val, SHORT onThresh) {
        SHORT offThresh = static_cast<SHORT>(onThresh * 3 / 4);
        int idxPos = KeyIndex(posKey);
        int idxNeg = KeyIndex(negKey);

        if (ValidIndex(idxPos)) {
            bool& active = keyStates[idxPos].isDown;
            active = active ? val > offThresh : val > onThresh;
        }
        if (ValidIndex(idxNeg)) {
            bool& active = keyStates[idxNeg].isDown;
            active = active ? val < -offThresh : val < -onThresh;
        }
    };
    setAxis(KJ_XBOX_LS_RIGHT, KJ_XBOX_LS_LEFT, gp.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    setAxis(KJ_XBOX_LS_UP,    KJ_XBOX_LS_DOWN, gp.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    setAxis(KJ_XBOX_RS_RIGHT, KJ_XBOX_RS_LEFT, gp.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
    setAxis(KJ_XBOX_RS_UP,    KJ_XBOX_RS_DOWN, gp.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
}

static void ClearJoystickStates()
{
    for (int key = KJ_JOYSTICK_POV_UP_LEFT; key <= KJ_JOYSTICK_AXIS_X_POS; ++key) {
        int idx = KeyIndex(key);
        if (ValidIndex(idx))
            keyStates[idx].isDown = false;
    }
}

static int AxisDeflection(DWORD val, DWORD min, DWORD max)
{
    if (max <= min) return static_cast<int>(val) - 32767;

    const long long centre = (static_cast<long long>(min) + static_cast<long long>(max)) / 2;
    const long long half   = (static_cast<long long>(max) - static_cast<long long>(min)) / 2;
    if (half <= 0) return 0;

    long long d = static_cast<long long>(val) - centre;
    if (d >  half) d =  half;
    if (d < -half) d = -half;

    return static_cast<int>((d * 32767) / half);
}

static void PollJoystick()
{
    if (!joystickConnected) {
        DWORD now = GetTickCount();
        if (now - joystickRetryTick < 1000) return;
        joystickRetryTick = now;
    }

    UINT joyId = 0;
    JOYINFOEX ji = {};
    ji.dwSize  = sizeof(ji);
    ji.dwFlags = JOY_RETURNALL;

    if (!KJ_Joystick_ReadFirstAvailable(&joyId, &ji)) {
        joystickConnected = false;
        KJ_Joystick_SetRealConnected(false);
        ClearJoystickStates();
        return;
    }
    joystickConnected = true;
    KJ_Joystick_SetRealConnected(true);

    JOYCAPSA caps = {};
    bool haveCaps = KJ_Joystick_GetCapsA(joyId, &caps);

    auto axisRange = [&](UINT mn, UINT mx, DWORD& lo, DWORD& hi) {
        if (haveCaps && mx > mn) { lo = mn; hi = mx; }
        else                     { lo = 0;  hi = 65535; }
    };
    DWORD xLo, xHi, yLo, yHi, zLo, zHi, rLo, rHi;
    axisRange(caps.wXmin, caps.wXmax, xLo, xHi);
    axisRange(caps.wYmin, caps.wYmax, yLo, yHi);
    axisRange(caps.wZmin, caps.wZmax, zLo, zHi);
    axisRange(caps.wRmin, caps.wRmax, rLo, rHi);

    auto setAxis = [&](int posKey, int negKey, DWORD val, DWORD mn, DWORD mx,
                       bool present, int deadzone) {
        if (!present) return;

        const int delta = AxisDeflection(val, mn, mx);
        const int off   = deadzone * 3 / 4;
        int idxPos = KeyIndex(posKey);
        int idxNeg = KeyIndex(negKey);

        if (ValidIndex(idxPos)) {
            bool& active = keyStates[idxPos].isDown;
            active = active ? delta > off : delta > deadzone;
        }
        if (ValidIndex(idxNeg)) {
            bool& active = keyStates[idxNeg].isDown;
            active = active ? delta < -off : delta < -deadzone;
        }
    };

    setAxis(KJ_JOYSTICK_AXIS_X_POS, KJ_JOYSTICK_AXIS_X_NEG, ji.dwXpos, xLo, xHi, true, KJ_AXIS_DEADZONE_X);
    setAxis(KJ_JOYSTICK_AXIS_Y_POS, KJ_JOYSTICK_AXIS_Y_NEG, ji.dwYpos, yLo, yHi, true, KJ_AXIS_DEADZONE_Y);
    setAxis(KJ_JOYSTICK_AXIS_Z_POS, KJ_JOYSTICK_AXIS_Z_NEG, ji.dwZpos, zLo, zHi, haveCaps && (caps.wCaps & JOYCAPS_HASZ), KJ_AXIS_DEADZONE_Z);
    setAxis(KJ_JOYSTICK_ROTATION_Z_POS, KJ_JOYSTICK_ROTATION_Z_NEG, ji.dwRpos, rLo, rHi, haveCaps && (caps.wCaps & JOYCAPS_HASR), KJ_AXIS_DEADZONE_R);

    // Buttons
    for (int i = 0; i < 16; ++i) {
        int idx = KeyIndex(KJ_JOYSTICK_BUTTON_1 - i);
        if (ValidIndex(idx))
            keyStates[idx].isDown = (ji.dwButtons & (1u << i)) != 0;
    }

    // POV
    static const DWORD s_povAngles[8] = { 0, 4500, 9000, 13500, 18000, 22500, 27000, 31500 };
    for (int i = 0; i < 8; ++i) {
        int idx = KeyIndex(KJ_JOYSTICK_POV_UP - i);
        if (!ValidIndex(idx)) continue;

        bool active = false;
        if (ji.dwPOV != JOY_POVCENTERED) {
            DWORD target = s_povAngles[i];
            DWORD pov = ji.dwPOV % 36000;
            DWORD diff = (pov >= target) ? (pov - target) : (36000 + pov - target);
            if (diff > 18000) diff = 36000 - diff;
            active = (diff < 2250);
        }
        keyStates[idx].isDown = active;
    }
}

void Poll_Devices()
{
    std::lock_guard<std::recursive_mutex> guard(mutex);

    PollKeyboard();
    PollMouse();
    PollWheel();
    if (KJ_Xbox_Enabled)
        PollXboxController();

    if (xInputConnected && KJ_Xbox_Enabled) {
        if (joystickConnected) {
            ClearJoystickStates();
            joystickConnected = false;
        }
    } else {
        if (KJ_Joystick_Enabled)
            PollJoystick();
        else {
            ClearJoystickStates();
            joystickConnected = false;
            }
    }
}

/**********************************************************Dispatch**********************************************************/

void Dispatch_Events()
{
    std::lock_guard<std::recursive_mutex> guard(mutex);

    HWND fg = kj_cachedForeground.load(std::memory_order_relaxed);
    HWND target = targetGameHwnd.load(std::memory_order_relaxed);

    if (fg != target) {
        for (int i = 0; i < KJ_KEY_ARRAY_SIZE; ++i)
            keyStates[i].wasDown = keyStates[i].isDown;

        mouseDeltaX.store(0, std::memory_order_relaxed);
        mouseDeltaY.store(0, std::memory_order_relaxed);
        GetCursorPos(&lastCursorPos);
        return;
    }

    int dx = mouseDeltaX.exchange(0, std::memory_order_relaxed);
    int dy = mouseDeltaY.exchange(0, std::memory_order_relaxed);
    if ((dx != 0 || dy != 0) && mouseMoveCb) {
        Invoke_Mouse_Move_Callback(mouseMoveCb, dx, dy);
        GetCursorPos(&lastCursorPos);
    }

    for (int i = 0; i < KJ_KEY_ARRAY_SIZE; ++i) {
        KeyState& s = keyStates[i];

        // The array is offset, so recover the real key code for error reporting.
        const int key = i - KJ_KEY_OFFSET;

        if (s.isDown && !s.wasDown && s.onPress)
            Invoke_Callback(s.onPress, "onPress", key);
        if (s.isDown && s.onHeld)
            Invoke_Callback(s.onHeld, "onHeld", key);
        if (!s.isDown && s.wasDown && s.onRelease)
            Invoke_Callback(s.onRelease, "onRelease", key);

        s.wasDown = s.isDown;
    }
}

void KJ_Controls_Reset()
{
    std::lock_guard<std::recursive_mutex> guard(mutex);

    // Fire a release for anything still held before the maps are dropped.
    for (int i = 0; i < KJ_KEY_ARRAY_SIZE; ++i) {
        KeyState& s = keyStates[i];
        if ((s.isDown || s.wasDown) && s.onRelease)
            Invoke_Callback(s.onRelease, "onRelease", i - KJ_KEY_OFFSET);
    }

    for (int i = 0; i < KJ_KEY_ARRAY_SIZE; ++i) {
        keyStates[i] = KeyState{};
        keyMapped[i].store(false, std::memory_order_relaxed);
    }

    KJ_ClearAllPhysicalDown();
    mouseDeltaX.store(0, std::memory_order_relaxed);
    mouseDeltaY.store(0, std::memory_order_relaxed);
    GetCursorPos(&lastCursorPos);
    xInputConnected = false;
    xInputRetryTick = 0;
    joystickConnected = false;
    joystickRetryTick = 0;
    KJ_Joystick_SetRealConnected(false);
}

void KJ_Controls_Map(int key, void(*onPress)(), void(*onHeld)(), void(*onRelease)())
{
    int idx = KeyIndex(key);
    if (!ValidIndex(idx)) return;

    std::lock_guard<std::recursive_mutex> guard(mutex);

    KeyState& s = keyStates[idx];
    s.onPress   = onPress;
    s.onHeld    = onHeld;
    s.onRelease = onRelease;
    s.isDown    = false;
    s.wasDown   = false;
    keyMapped[idx].store(true, std::memory_order_relaxed);
}

void KJ_Controls_SetMouseMoveCallback(void(*callback)(int, int))
{
    mouseMoveCb = callback;
}
