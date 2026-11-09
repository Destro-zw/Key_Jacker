#pragma once

#include <windows.h>
#include <mmsystem.h>

enum class KJ_JoystickAxis : int { X, Y, Z, R, U, V };

enum class KJ_JoystickGetType : int {
    X, Y, Z, R, U, V, POV
};

void KJ_Joystick_Install();
void KJ_Joystick_Uninstall();

// Reads joystick state via the real WinMM function, bypassing the IAT hook.
bool KJ_Joystick_ReadReal(UINT uJoyID, LPJOYINFOEX pji);

// Finds the first available joystick ID and reads its state.
bool KJ_Joystick_ReadFirstAvailable(UINT* outId, LPJOYINFOEX pji);

// Gets joystick capabilities via the real WinMM function.
bool KJ_Joystick_GetCapsA(UINT uJoyID, LPJOYCAPSA pjc);

void KJ_Joystick_SetAxis(KJ_JoystickAxis axis, DWORD value);
void KJ_Joystick_SetButton(int buttonIndex, bool down);
void KJ_Joystick_SetPOV(DWORD pov);
void KJ_Joystick_Reset();

void KJ_Joystick_SetRealConnected(bool connected);

void KJ_Joystick_Calabrate_Center();
DWORD KJ_Joystick_Get_Center();
    
bool KJ_Joystick_GetAxis(KJ_JoystickGetType type,
                         UINT* outId, DWORD* outMin, DWORD* outMax, DWORD* outPos);

// Sets the deadzone for a joystick axis.
bool KJ_Joystick_SetDeadzone(KJ_JoystickGetType type, int range);

// Reads back the value last set with KJ_Joystick_SetDeadzone.
bool KJ_Joystick_GetDeadzone(KJ_JoystickGetType type, int* outRange);