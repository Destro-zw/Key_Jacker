#pragma once

#include <windows.h>
#include <atomic>

// So the low-level hooks can tell our own events apart from the
// user's and let them pass through instead of blocking them.
inline constexpr ULONG_PTR KEYJ_INJECT_TAG = 0x4B45594A; // "KEYJ"

// Events are only dispatched while this window owns the foreground.
inline std::atomic<HWND> targetGameHwnd{nullptr};

// Foreground window cached by the hook thread, avoiding a Win32 call per event.
inline std::atomic<HWND> kj_cachedForeground{nullptr};
