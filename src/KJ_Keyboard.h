#pragma once

#include "KJ_Global.h"
#include "KJ_Controls.h"

bool KJ_Keyboard_Install(HINSTANCE hInstance);
void KJ_Keyboard_Uninstall();

void KJ_Keyboard_KeyPress(WORD vk);
void KJ_Keyboard_KeyHold(WORD vk);
void KJ_Keyboard_KeyRelease(WORD vk);