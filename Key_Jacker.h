#pragma once

// Public entry point for Key_Jacker.
// Include this header and call KJ_Install() / KJ_Uninstall() to manage the
// background input pipeline.

#include <windows.h>

#include "src/KJ_Global.h"
#include "src/KJ_Controls.h"
#include "src/KJ_Keyboard.h"
#include "src/KJ_Mouse.h"
#include "src/KJ_Joystick.h"

// Installs the hooks and starts the polling/dispatch thread.
//   hInstance  - module handle of the calling application (used for the hooks).
//   targetHwnd - window that must be foreground for events to be dispatched.
void KJ_Install(HINSTANCE hInstance = nullptr, HWND targetHwnd = nullptr);

// Stops the threads, removes the hooks and clears all mappings.
void KJ_Uninstall();

bool KJ_Joystick_Get_Enabled();
void KJ_Joystick_Set_Enabled(bool enable);

bool KJ_Xbox_Get_Enabled();
void KJ_Xbox_Set_Enabled(bool enable);