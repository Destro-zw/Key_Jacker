#include <windows.h>
#include <cstring>

#include <atomic>
#include <mutex>

#include "KJ_Controls.h"
#include "KJ_Joystick.h"

// State

static std::atomic<DWORD> CENTER{(DWORD)32767};

static inline DWORD Center()
{
    return CENTER.load(std::memory_order_relaxed);
}

static std::mutex lock;
static std::atomic<bool> installed{false};
static std::atomic<bool> realConnected{false};

struct VirtualState {
    DWORD x = Center(), y = Center(), z = Center();
    DWORD r = Center(), u = Center(), v = Center();
    DWORD buttons = 0;
    DWORD pov = JOY_POVCENTERED;
};
static VirtualState virt;

// Real WinMM function pointers

static HMODULE hWinMM = nullptr;

using fn_joyGetNumDevs  = UINT   (WINAPI*)(void);
using fn_joyGetPos      = MMRESULT(WINAPI*)(UINT, LPJOYINFO);
using fn_joyGetPosEx    = MMRESULT(WINAPI*)(UINT, LPJOYINFOEX);
using fn_joyGetDevCapsA = MMRESULT(WINAPI*)(UINT, LPJOYCAPSA, UINT);
using fn_joyGetDevCapsW = MMRESULT(WINAPI*)(UINT, LPJOYCAPSW, UINT);

static fn_joyGetNumDevs  real_joyGetNumDevs  = nullptr;
static fn_joyGetPos      real_joyGetPos      = nullptr;
static fn_joyGetPosEx    real_joyGetPosEx    = nullptr;
static fn_joyGetDevCapsA real_joyGetDevCapsA = nullptr;
static fn_joyGetDevCapsW real_joyGetDevCapsW = nullptr;

// IAT helpers

static void PatchIAT(HMODULE hModule, const char* targetDll,
                     const char* funcName, FARPROC newAddr,
                     FARPROC* outOriginal = nullptr)
{
    if (!hModule || !targetDll || !funcName || !newAddr) return;

    auto base = reinterpret_cast<unsigned char*>(hModule);
    auto dos  = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;

    auto nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;

    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (dir.VirtualAddress == 0 || dir.Size == 0) return;

    auto imports = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(base + dir.VirtualAddress);
    for (; imports->Name; ++imports) {
        auto dllName = reinterpret_cast<const char*>(base + imports->Name);
        if (_stricmp(dllName, targetDll) != 0) continue;

        auto orig  = reinterpret_cast<PIMAGE_THUNK_DATA>(base + imports->OriginalFirstThunk);
        auto thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(base + imports->FirstThunk);

        for (; orig->u1.AddressOfData; ++orig, ++thunk) {
            if (orig->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            auto ibn = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(base + orig->u1.AddressOfData);
            if (_stricmp(reinterpret_cast<const char*>(ibn->Name), funcName) != 0) continue;

            DWORD oldProtect;
            VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect);
            if (outOriginal) *outOriginal = reinterpret_cast<FARPROC>(thunk->u1.Function);
            thunk->u1.Function = reinterpret_cast<ULONG_PTR>(newAddr);
            VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
            return; // first match wins
        }
    }
}

struct HookEntry {
    const char* name;
    FARPROC     hook;
    FARPROC     saved;
};

// Declared up front so the table can hold the hook addresses.
static UINT WINAPI Hooked_joyGetNumDevs();
static MMRESULT WINAPI Hooked_joyGetPos(UINT uJoyID, LPJOYINFO pji);
static MMRESULT WINAPI Hooked_joyGetPosEx(UINT uJoyID, LPJOYINFOEX pji);
static MMRESULT WINAPI Hooked_joyGetDevCapsA(UINT uJoyID, LPJOYCAPSA pjc, UINT cbjc);
static MMRESULT WINAPI Hooked_joyGetDevCapsW(UINT uJoyID, LPJOYCAPSW pjc, UINT cbjc);

static HookEntry hooks[] = {
    { "joyGetNumDevs",  reinterpret_cast<FARPROC>(Hooked_joyGetNumDevs),  nullptr },
    { "joyGetPos",      reinterpret_cast<FARPROC>(Hooked_joyGetPos),      nullptr },
    { "joyGetPosEx",    reinterpret_cast<FARPROC>(Hooked_joyGetPosEx),    nullptr },
    { "joyGetDevCapsA", reinterpret_cast<FARPROC>(Hooked_joyGetDevCapsA), nullptr },
    { "joyGetDevCapsW", reinterpret_cast<FARPROC>(Hooked_joyGetDevCapsW), nullptr },
};

static void PatchMainModule()
{
    HMODULE hMain = GetModuleHandle(nullptr);
    for (auto& h : hooks)
        PatchIAT(hMain, "winmm.dll", h.name, h.hook, &h.saved);
}

static void UnpatchMainModule()
{
    HMODULE hMain = GetModuleHandle(nullptr);
    for (auto& h : hooks) {
        if (h.saved)
            PatchIAT(hMain, "winmm.dll", h.name, h.saved);
        h.saved = nullptr;
    }
}

// Blocking mapped inputs

static bool AxisMapped(int posKey, int negKey)
{
    return KJ_Controls_IsMapped(posKey) || KJ_Controls_IsMapped(negKey);
}

static bool AnyPovMapped()
{
    for (int key = KJ_JOYSTICK_POV_UP_LEFT; key <= KJ_JOYSTICK_POV_UP; ++key)
        if (KJ_Controls_IsMapped(key)) return true;
    return false;
}

static void BlockMappedInputs(LPJOYINFO pji)
{
    if (!pji) return;

    if (AxisMapped(KJ_JOYSTICK_AXIS_X_POS, KJ_JOYSTICK_AXIS_X_NEG)) pji->wXpos = static_cast<WORD>(Center());
    if (AxisMapped(KJ_JOYSTICK_AXIS_Y_POS, KJ_JOYSTICK_AXIS_Y_NEG)) pji->wYpos = static_cast<WORD>(Center());
    if (AxisMapped(KJ_JOYSTICK_AXIS_Z_POS, KJ_JOYSTICK_AXIS_Z_NEG)) pji->wZpos = static_cast<WORD>(Center());

    for (int i = 0; i < 16; ++i)
        if (KJ_Controls_IsMapped(KJ_JOYSTICK_BUTTON_1 - i))
            pji->wButtons &= ~static_cast<WORD>(1 << i);
}

static void BlockMappedInputsEx(LPJOYINFOEX pji)
{
    if (!pji) return;

    if (AxisMapped(KJ_JOYSTICK_AXIS_X_POS, KJ_JOYSTICK_AXIS_X_NEG)) pji->dwXpos = Center();
    if (AxisMapped(KJ_JOYSTICK_AXIS_Y_POS, KJ_JOYSTICK_AXIS_Y_NEG)) pji->dwYpos = Center();
    if (AxisMapped(KJ_JOYSTICK_AXIS_Z_POS, KJ_JOYSTICK_AXIS_Z_NEG)) pji->dwZpos = Center();
    if (AxisMapped(KJ_JOYSTICK_ROTATION_Z_POS, KJ_JOYSTICK_ROTATION_Z_NEG)) pji->dwRpos = Center();

    for (int i = 0; i < 16; ++i)
        if (KJ_Controls_IsMapped(KJ_JOYSTICK_BUTTON_1 - i))
            pji->dwButtons &= ~(1u << i);

    if (AnyPovMapped())
        pji->dwPOV = JOY_POVCENTERED;
}

static void InjectVirtualState(LPJOYINFO pji)
{
    std::lock_guard<std::mutex> guard(lock);
    if (virt.x != Center()) pji->wXpos = static_cast<WORD>(virt.x & 0xFFFF);
    if (virt.y != Center()) pji->wYpos = static_cast<WORD>(virt.y & 0xFFFF);
    if (virt.z != Center()) pji->wZpos = static_cast<WORD>(virt.z & 0xFFFF);
    pji->wButtons |= static_cast<WORD>(virt.buttons & 0xFFFF);
}

static void InjectVirtualStateEx(LPJOYINFOEX pji)
{
    std::lock_guard<std::mutex> guard(lock);
    if (virt.x != Center()) pji->dwXpos = virt.x;
    if (virt.y != Center()) pji->dwYpos = virt.y;
    if (virt.z != Center()) pji->dwZpos = virt.z;
    if (virt.r != Center()) pji->dwRpos = virt.r;
    if (virt.u != Center()) pji->dwUpos = virt.u;
    if (virt.v != Center()) pji->dwVpos = virt.v;
    pji->dwButtons |= virt.buttons;
    if (virt.pov != JOY_POVCENTERED) pji->dwPOV = virt.pov;
}

// Hooked WinMM functions

static UINT WINAPI Hooked_joyGetNumDevs()
{
    if (!installed.load(std::memory_order_acquire))
        return real_joyGetNumDevs ? real_joyGetNumDevs() : 0;

    UINT real = real_joyGetNumDevs ? real_joyGetNumDevs() : 0;
    return real ? real : 1;           // always report at least one
}

static MMRESULT WINAPI Hooked_joyGetPos(UINT uJoyID, LPJOYINFO pji)
{
    if (!pji) return MMSYSERR_INVALPARAM;
    if (!installed.load(std::memory_order_acquire))
        return real_joyGetPos ? real_joyGetPos(uJoyID, pji) : MMSYSERR_INVALPARAM;
    bool haveReal = false;
    if (realConnected.load(std::memory_order_acquire))
        haveReal = real_joyGetPos && (real_joyGetPos(uJoyID, pji) == MMSYSERR_NOERROR);
    if (!haveReal) {
        pji->wXpos = static_cast<WORD>(Center());
        pji->wYpos = static_cast<WORD>(Center());
        pji->wZpos = static_cast<WORD>(Center());
        pji->wButtons = 0;
    }
    BlockMappedInputs(pji);
    InjectVirtualState(pji);
    return MMSYSERR_NOERROR;
}

static MMRESULT WINAPI Hooked_joyGetPosEx(UINT uJoyID, LPJOYINFOEX pji)
{
    if (!pji || pji->dwSize < sizeof(JOYINFOEX)) return MMSYSERR_INVALPARAM;
    if (!installed.load(std::memory_order_acquire))
        return real_joyGetPosEx ? real_joyGetPosEx(uJoyID, pji) : MMSYSERR_INVALPARAM;
    bool haveReal = false;
    if (realConnected.load(std::memory_order_acquire))
        haveReal = real_joyGetPosEx && (real_joyGetPosEx(uJoyID, pji) == MMSYSERR_NOERROR);
    if (!haveReal) {
        pji->dwXpos = Center(); pji->dwYpos = Center(); pji->dwZpos = Center();
        pji->dwRpos = Center(); pji->dwUpos = Center(); pji->dwVpos = Center();
        pji->dwButtons = 0;
        pji->dwPOV = JOY_POVCENTERED;
    }
    BlockMappedInputsEx(pji);
    InjectVirtualStateEx(pji);
    return MMSYSERR_NOERROR;
}

// Only the caps type and the matching real function differ between A and W.
template <typename CAPS, typename RealFn>
static MMRESULT FillDevCaps(UINT uJoyID, CAPS* pjc, UINT cbjc, RealFn real)
{
    if (!pjc || cbjc < sizeof(CAPS)) return MMSYSERR_INVALPARAM;
    if (!installed.load(std::memory_order_acquire))
        return real ? real(uJoyID, pjc, cbjc) : MMSYSERR_INVALPARAM;
    if (realConnected.load(std::memory_order_acquire) &&
        real && real(uJoyID, pjc, cbjc) == MMSYSERR_NOERROR)
        return MMSYSERR_NOERROR;

    ZeroMemory(pjc, cbjc);
    pjc->wXmin = 0;   pjc->wXmax = 65535;
    pjc->wYmin = 0;   pjc->wYmax = 65535;
    pjc->wZmin = 0;   pjc->wZmax = 65535;
    pjc->wNumAxes = 3; pjc->wMaxAxes = 6; pjc->wNumButtons = 32;
    return MMSYSERR_NOERROR;
}

static MMRESULT WINAPI Hooked_joyGetDevCapsA(UINT uJoyID, LPJOYCAPSA pjc, UINT cbjc)
{
    return FillDevCaps(uJoyID, pjc, cbjc, real_joyGetDevCapsA);
}

static MMRESULT WINAPI Hooked_joyGetDevCapsW(UINT uJoyID, LPJOYCAPSW pjc, UINT cbjc)
{
    return FillDevCaps(uJoyID, pjc, cbjc, real_joyGetDevCapsW);
}

void KJ_Joystick_Install()
{
    if (installed.load(std::memory_order_acquire)) return;

    if (!hWinMM) {
        hWinMM = LoadLibraryA("winmm.dll");
        if (hWinMM) {
            real_joyGetNumDevs  = reinterpret_cast<fn_joyGetNumDevs> (GetProcAddress(hWinMM, "joyGetNumDevs"));
            real_joyGetPos      = reinterpret_cast<fn_joyGetPos>     (GetProcAddress(hWinMM, "joyGetPos"));
            real_joyGetPosEx    = reinterpret_cast<fn_joyGetPosEx>   (GetProcAddress(hWinMM, "joyGetPosEx"));
            real_joyGetDevCapsA = reinterpret_cast<fn_joyGetDevCapsA>(GetProcAddress(hWinMM, "joyGetDevCapsA"));
            real_joyGetDevCapsW = reinterpret_cast<fn_joyGetDevCapsW>(GetProcAddress(hWinMM, "joyGetDevCapsW"));
        }
    }

    realConnected.store(false, std::memory_order_release);
    installed.store(true, std::memory_order_release);
    KJ_Joystick_Reset();
    PatchMainModule();
}

void KJ_Joystick_Uninstall()
{
    if (!installed.load(std::memory_order_acquire)) return;

    KJ_Joystick_Reset();
    realConnected.store(false, std::memory_order_release);
    installed.store(false, std::memory_order_release);
    UnpatchMainModule();
}

void KJ_Joystick_SetAxis(KJ_JoystickAxis axis, DWORD value)
{
    if (!installed.load(std::memory_order_acquire)) return;
    std::lock_guard<std::mutex> guard(lock);
    switch (axis) {
        case KJ_JoystickAxis::X: virt.x = value; break;
        case KJ_JoystickAxis::Y: virt.y = value; break;
        case KJ_JoystickAxis::Z: virt.z = value; break;
        case KJ_JoystickAxis::R: virt.r = value; break;
        case KJ_JoystickAxis::U: virt.u = value; break;
        case KJ_JoystickAxis::V: virt.v = value; break;
    }
}

void KJ_Joystick_SetButton(int buttonIndex, bool down)
{
    if (!installed.load(std::memory_order_acquire)) return;
    std::lock_guard<std::mutex> guard(lock);
    if (down) virt.buttons |=  (1u << buttonIndex);
    else      virt.buttons &= ~(1u << buttonIndex);
}

void KJ_Joystick_SetPOV(DWORD pov)
{
    if (!installed.load(std::memory_order_acquire)) return;
    std::lock_guard<std::mutex> guard(lock);
    virt.pov = pov;
}

void KJ_Joystick_SetRealConnected(bool connected)
{
    realConnected.store(connected, std::memory_order_release);
}

void KJ_Joystick_Reset()
{
    if (!installed.load(std::memory_order_acquire)) return;
    std::lock_guard<std::mutex> guard(lock);
    virt = VirtualState{};
}

bool KJ_Joystick_ReadReal(UINT uJoyID, LPJOYINFOEX pji)
{
    if (!pji) return false;
    if (real_joyGetPosEx)
        return real_joyGetPosEx(uJoyID, pji) == MMSYSERR_NOERROR;
    return joyGetPosEx(uJoyID, pji) == MMSYSERR_NOERROR;
}

bool KJ_Joystick_GetCapsA(UINT uJoyID, LPJOYCAPSA pjc)
{
    if (!pjc) return false;
    if (real_joyGetDevCapsA)
        return real_joyGetDevCapsA(uJoyID, pjc, sizeof(JOYCAPSA)) == MMSYSERR_NOERROR;
    return joyGetDevCapsA(uJoyID, pjc, sizeof(JOYCAPSA)) == MMSYSERR_NOERROR;
}

bool KJ_Joystick_ReadFirstAvailable(UINT* outId, LPJOYINFOEX pji)
{
    if (!pji || pji->dwSize < sizeof(JOYINFOEX)) return false;

    static UINT s_cachedId = (UINT)-1;

    UINT numDevs = real_joyGetNumDevs ? real_joyGetNumDevs() : 0;
    if (numDevs == 0) return false;

    // Reads one device, skipping the DualSense pads that XInput owns.
    auto tryRead = [&](UINT id) {
        JOYCAPSA caps = {};
        if (!KJ_Joystick_GetCapsA(id, &caps)) return false;
        if (caps.wMid == 0x054C && (caps.wPid == 0x0CE6 || caps.wPid == 0x0DF2)) return false;

        JOYINFOEX info = {};
        info.dwSize  = sizeof(info);
        info.dwFlags = pji->dwFlags;
        if (!KJ_Joystick_ReadReal(id, &info)) return false;

        s_cachedId = id;
        *pji = info;
        if (outId) *outId = id;
        return true;
    };

    if (s_cachedId < numDevs && tryRead(s_cachedId)) return true;

    for (UINT id = 0; id < numDevs; ++id)
        if (id != s_cachedId && tryRead(id)) return true;

    s_cachedId = (UINT)-1;
    return false;
}

struct AxisInfo {
    size_t offMin;
    size_t offMax;
    size_t offPos;
};

static constexpr AxisInfo kAxisInfo[] = {
    /* X */ { offsetof(JOYCAPSA, wXmin), offsetof(JOYCAPSA, wXmax), offsetof(JOYINFOEX, dwXpos) },
    /* Y */ { offsetof(JOYCAPSA, wYmin), offsetof(JOYCAPSA, wYmax), offsetof(JOYINFOEX, dwYpos) },
    /* Z */ { offsetof(JOYCAPSA, wZmin), offsetof(JOYCAPSA, wZmax), offsetof(JOYINFOEX, dwZpos) },
    /* R */ { offsetof(JOYCAPSA, wRmin), offsetof(JOYCAPSA, wRmax), offsetof(JOYINFOEX, dwRpos) },
    /* U */ { offsetof(JOYCAPSA, wUmin), offsetof(JOYCAPSA, wUmax), offsetof(JOYINFOEX, dwUpos) },
    /* V */ { offsetof(JOYCAPSA, wVmin), offsetof(JOYCAPSA, wVmax), offsetof(JOYINFOEX, dwVpos) },
};

static_assert(static_cast<int>(KJ_JoystickGetType::X) == 0 &&
              static_cast<int>(KJ_JoystickGetType::POV) == 6,
              "KJ_JoystickGetType layout mismatch");

bool KJ_Joystick_GetAxis(KJ_JoystickGetType type,
                         UINT* outId, DWORD* outMin, DWORD* outMax, DWORD* outPos)
{
    if (!outMin || !outMax || !outPos) return false;

    UINT joyId = 0;
    JOYINFOEX ji = {};
    ji.dwSize  = sizeof(ji);
    ji.dwFlags = JOY_RETURNALL;
    if (!KJ_Joystick_ReadFirstAvailable(&joyId, &ji)) return false;

    int t = static_cast<int>(type);

    // POV
    if (type == KJ_JoystickGetType::POV) {
        *outMin = 0;
        *outMax = 35900;
        *outPos = ji.dwPOV;
        if (outId) *outId = joyId;
        return true;
    }

    if (t < 0 || t >= 6) return false;

    JOYCAPSA caps = {};
    if (KJ_Joystick_GetCapsA(joyId, &caps)) {
        const AxisInfo& info = kAxisInfo[t];
        *outMin = *reinterpret_cast<DWORD*>(reinterpret_cast<char*>(&caps) + info.offMin);
        *outMax = *reinterpret_cast<DWORD*>(reinterpret_cast<char*>(&caps) + info.offMax);
        *outPos = *reinterpret_cast<DWORD*>(reinterpret_cast<char*>(&ji)   + info.offPos);
    } else {
        *outMin = 0;
        *outMax = 65535;
        *outPos = *reinterpret_cast<DWORD*>(reinterpret_cast<char*>(&ji) + kAxisInfo[t].offPos);
    }
    if (outId) *outId = joyId;
    return true;
}

// Deadzone

static int* DeadzonePtr(KJ_JoystickGetType type)
{
    switch (type) {
        case KJ_JoystickGetType::X: return &KJ_AXIS_DEADZONE_X;
        case KJ_JoystickGetType::Y: return &KJ_AXIS_DEADZONE_Y;
        case KJ_JoystickGetType::Z: return &KJ_AXIS_DEADZONE_Z;
        case KJ_JoystickGetType::R: return &KJ_AXIS_DEADZONE_R;
        default: return nullptr;
    }
}

bool KJ_Joystick_SetDeadzone(KJ_JoystickGetType type, int range)
{
    int* p = DeadzonePtr(type);
    if (!p) return false;
    if (range < 0) range = 0;
    *p = range;
    return true;
}

bool KJ_Joystick_GetDeadzone(KJ_JoystickGetType type, int* outRange)
{
    int* p = DeadzonePtr(type);
    if (!p || !outRange) return false;
    *outRange = *p;
    return true;
}

void KJ_Joystick_Calabrate_Center()
{
    DWORD mn = 0, mx = 65535, pos = 0;
    if (!KJ_Joystick_GetAxis(KJ_JoystickGetType::X, nullptr, &mn, &mx, &pos))
        return;

    DWORD centre = (mn + mx) / 2;
    CENTER.store(centre, std::memory_order_release);
}

DWORD KJ_Joystick_Get_Center()
{
    return (DWORD) CENTER;
}