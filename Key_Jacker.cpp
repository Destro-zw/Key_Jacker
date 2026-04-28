#include "Key_Jacker.h"

#include <atomic>
#include <thread>

namespace {

constexpr DWORD POLL_INTERVAL_MS = 4;

std::atomic<bool> inputThreadStop{false};
std::atomic<bool> hookThreadReady{false};
std::atomic<bool> installed{false};

HINSTANCE hookHInstance = nullptr;
std::thread hookPumpThread;
std::thread pollDispatchThread;

std::thread SpawnThread(void (*proc)(), int priority)
{
    std::thread t(proc);
    SetThreadPriority(t.native_handle(), priority);
    return t;
}

void HookPumpThreadProc()
{
    MSG msg;

    // Create this thread's message queue before installing the hooks.
    PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE);
    kj_cachedForeground.store(::GetForegroundWindow(), std::memory_order_relaxed);

    KJ_Keyboard_Install(hookHInstance);
    KJ_Mouse_Install(hookHInstance);
    hookThreadReady.store(true, std::memory_order_release);

    while (!inputThreadStop.load(std::memory_order_acquire))
    {
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 50, QS_ALLINPUT);
        kj_cachedForeground.store(::GetForegroundWindow(), std::memory_order_relaxed);

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    KJ_Keyboard_Uninstall();
    KJ_Mouse_Uninstall();
}

void PollDispatchThreadProc()
{
    timeBeginPeriod(1);
    KJ_InputThreadActive.store(true, std::memory_order_release);

    while (!inputThreadStop.load(std::memory_order_acquire))
    {
        Sleep(POLL_INTERVAL_MS);
        Poll_Devices();
        Dispatch_Events();
    }

    KJ_InputThreadActive.store(false, std::memory_order_release);
    timeEndPeriod(1);
}

} // namespace

void KJ_Install(HINSTANCE hInstance, HWND targetHwnd)
{
    if (installed.exchange(true)) return;

    targetGameHwnd.store(targetHwnd, std::memory_order_release);
    hookHInstance = hInstance;
    inputThreadStop.store(false, std::memory_order_release);
    hookThreadReady.store(false, std::memory_order_release);

    KJ_Controls_Reset();

    hookPumpThread = SpawnThread(HookPumpThreadProc, THREAD_PRIORITY_HIGHEST);

    while (!hookThreadReady.load(std::memory_order_acquire))
        Sleep(1);

    pollDispatchThread = SpawnThread(PollDispatchThreadProc, THREAD_PRIORITY_ABOVE_NORMAL);

    KJ_Joystick_Install();
}

void KJ_Uninstall()
{
    if (!installed.exchange(false)) return;

    inputThreadStop.store(true, std::memory_order_release);

    if (hookPumpThread.joinable())
        hookPumpThread.join();
    if (pollDispatchThread.joinable())
        pollDispatchThread.join();

    targetGameHwnd.store(nullptr, std::memory_order_release);
    KJ_Joystick_Uninstall();
    KJ_Controls_Reset();
}
