#pragma once

#include "KJ_Global.h"
#include "KJ_Controls.h"

bool KJ_Mouse_Install(HINSTANCE hInstance);
void KJ_Mouse_Uninstall();

// Synthesised mouse input (for remapping / virtual device output).
void KJ_Mouse_ButtonClick(int button);
void KJ_Mouse_ButtonHold(int button);
void KJ_Mouse_ButtonRelease(int button);
void KJ_Mouse_Wheel(int notches); // +1 = one notch up, -1 = one notch down
void KJ_Mouse_Move(int deltaX, int deltaY);