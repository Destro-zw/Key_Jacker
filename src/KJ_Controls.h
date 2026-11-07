#pragma once

#include <windows.h>
#include <atomic>

// ---------------------------------------------------------------------------
// Key codes
// ---------------------------------------------------------------------------

// --- Keyboard ---------------------------------------------------------------
constexpr int KJ_KEY_ESCAPE     = VK_ESCAPE;
constexpr int KJ_KEY_SPACE      = VK_SPACE;
constexpr int KJ_KEY_ENTER      = VK_RETURN;
constexpr int KJ_KEY_TAB        = VK_TAB;
constexpr int KJ_KEY_BACKSPACE  = VK_BACK;
constexpr int KJ_KEY_SHIFT_L    = VK_LSHIFT;
constexpr int KJ_KEY_SHIFT_R    = VK_RSHIFT;
constexpr int KJ_KEY_CTRL_L     = VK_LCONTROL;
constexpr int KJ_KEY_CTRL_R     = VK_RCONTROL;
constexpr int KJ_KEY_ALT        = VK_LMENU;
constexpr int KJ_KEY_CAPSLOCK   = VK_CAPITAL;
constexpr int KJ_KEY_INSERT     = VK_INSERT;
constexpr int KJ_KEY_DELETE     = VK_DELETE;
constexpr int KJ_KEY_HOME       = VK_HOME;
constexpr int KJ_KEY_END        = VK_END;
constexpr int KJ_KEY_PAGEUP     = VK_PRIOR;
constexpr int KJ_KEY_PAGEDOWN   = VK_NEXT;
constexpr int KJ_KEY_UP         = VK_UP;
constexpr int KJ_KEY_DOWN       = VK_DOWN;
constexpr int KJ_KEY_LEFT       = VK_LEFT;
constexpr int KJ_KEY_RIGHT      = VK_RIGHT;
constexpr int KJ_KEY_BRACKET_LEFT  = VK_OEM_4;
constexpr int KJ_KEY_BRACKET_RIGHT = VK_OEM_6;
constexpr int KJ_KEY_HELP       = VK_HELP;
constexpr int KJ_KEY_A = 0x41;
constexpr int KJ_KEY_B = 0x42;
constexpr int KJ_KEY_C = 0x43;
constexpr int KJ_KEY_D = 0x44;
constexpr int KJ_KEY_E = 0x45;
constexpr int KJ_KEY_F = 0x46;
constexpr int KJ_KEY_G = 0x47;
constexpr int KJ_KEY_H = 0x48;
constexpr int KJ_KEY_I = 0x49;
constexpr int KJ_KEY_J = 0x4A;
constexpr int KJ_KEY_K = 0x4B;
constexpr int KJ_KEY_L = 0x4C;
constexpr int KJ_KEY_M = 0x4D;
constexpr int KJ_KEY_N = 0x4E;
constexpr int KJ_KEY_O = 0x4F;
constexpr int KJ_KEY_P = 0x50;
constexpr int KJ_KEY_Q = 0x51;
constexpr int KJ_KEY_R = 0x52;
constexpr int KJ_KEY_S = 0x53;
constexpr int KJ_KEY_T = 0x54;
constexpr int KJ_KEY_U = 0x55;
constexpr int KJ_KEY_V = 0x56;
constexpr int KJ_KEY_W = 0x57;
constexpr int KJ_KEY_X = 0x58;
constexpr int KJ_KEY_Y = 0x59;
constexpr int KJ_KEY_Z = 0x5A;
constexpr int KJ_KEY_0 = 0x30;
constexpr int KJ_KEY_1 = 0x31;
constexpr int KJ_KEY_2 = 0x32;
constexpr int KJ_KEY_3 = 0x33;
constexpr int KJ_KEY_4 = 0x34;
constexpr int KJ_KEY_5 = 0x35;
constexpr int KJ_KEY_6 = 0x36;
constexpr int KJ_KEY_7 = 0x37;
constexpr int KJ_KEY_8 = 0x38;
constexpr int KJ_KEY_9 = 0x39;
constexpr int KJ_KEY_F1  = VK_F1;
constexpr int KJ_KEY_F2  = VK_F2;
constexpr int KJ_KEY_F3  = VK_F3;
constexpr int KJ_KEY_F4  = VK_F4;
constexpr int KJ_KEY_F5  = VK_F5;
constexpr int KJ_KEY_F6  = VK_F6;
constexpr int KJ_KEY_F7  = VK_F7;
constexpr int KJ_KEY_F8  = VK_F8;
constexpr int KJ_KEY_F9  = VK_F9;
constexpr int KJ_KEY_F10 = VK_F10;
constexpr int KJ_KEY_F11 = VK_F11;
constexpr int KJ_KEY_F12 = VK_F12;
constexpr int KJ_KEY_PRINTSCREEN        = VK_SNAPSHOT;
constexpr int KJ_KEY_SCROLLLOCK         = VK_SCROLL;
constexpr int KJ_KEY_PAUSE              = VK_PAUSE;
constexpr int KJ_KEY_NUMLOCK            = VK_NUMLOCK;
constexpr int KJ_KEY_NUMPAD_DIVIDE      = VK_DIVIDE;
constexpr int KJ_KEY_NUMPAD_MULTIPLY    = VK_MULTIPLY;
constexpr int KJ_KEY_NUMPAD_MINUS       = VK_SUBTRACT;
constexpr int KJ_KEY_NUMPAD_PLUS        = VK_ADD;
constexpr int KJ_KEY_NUMPAD_ENTER       = VK_RETURN;
constexpr int KJ_KEY_COMMA              = VK_OEM_COMMA;
constexpr int KJ_KEY_PERIOD             = VK_OEM_PERIOD;
constexpr int KJ_KEY_SLASH              = VK_OEM_2;
constexpr int KJ_KEY_TILDE              = VK_OEM_3;
constexpr int KJ_KEY_MINUS              = VK_OEM_MINUS;
constexpr int KJ_KEY_EQUALS             = VK_OEM_PLUS;
constexpr int KJ_KEY_NUM_5              = VK_NUMPAD5;

// --- Mouse ------------------------------------------------------------------
constexpr int KJ_MOUSE_LEFT      = VK_LBUTTON;
constexpr int KJ_MOUSE_MIDDLE    = VK_MBUTTON;
constexpr int KJ_MOUSE_RIGHT     = VK_RBUTTON;
constexpr int KJ_MOUSE_X1        = VK_XBUTTON1;
constexpr int KJ_MOUSE_X2        = VK_XBUTTON2;
constexpr int KJ_MOUSE_WHEEL_UP   = -1;
constexpr int KJ_MOUSE_WHEEL_DOWN = -2;
constexpr int KJ_MOUSE_MOVE       = -8;

// --- Xbox (XInput) ----------------------------------------------------------
constexpr int KJ_XBOX_A        = -10;
constexpr int KJ_XBOX_B        = -11;
constexpr int KJ_XBOX_X        = -12;
constexpr int KJ_XBOX_Y        = -13;
constexpr int KJ_XBOX_LB       = -14;
constexpr int KJ_XBOX_RB       = -15;
constexpr int KJ_XBOX_LS_CLICK = -16;
constexpr int KJ_XBOX_RS_CLICK = -17;
constexpr int KJ_XBOX_BACK     = -18;
constexpr int KJ_XBOX_START    = -19;
constexpr int KJ_XBOX_DPAD_UP    = -20;
constexpr int KJ_XBOX_DPAD_DOWN  = -21;
constexpr int KJ_XBOX_DPAD_LEFT  = -22;
constexpr int KJ_XBOX_DPAD_RIGHT = -23;
constexpr int KJ_XBOX_LT = -24;
constexpr int KJ_XBOX_RT = -25;
constexpr int KJ_XBOX_LS_UP     = -26;
constexpr int KJ_XBOX_LS_DOWN   = -27;
constexpr int KJ_XBOX_LS_LEFT   = -28;
constexpr int KJ_XBOX_LS_RIGHT  = -29;
constexpr int KJ_XBOX_RS_UP     = -30;
constexpr int KJ_XBOX_RS_DOWN   = -31;
constexpr int KJ_XBOX_RS_LEFT   = -32;
constexpr int KJ_XBOX_RS_RIGHT  = -33;

// --- Joystick (WinMM) -------------------------------------------------------
constexpr int KJ_JOYSTICK_AXIS_X_POS        = -40;
constexpr int KJ_JOYSTICK_AXIS_X_NEG        = -41;
constexpr int KJ_JOYSTICK_AXIS_Y_POS        = -42;
constexpr int KJ_JOYSTICK_AXIS_Y_NEG        = -43;
constexpr int KJ_JOYSTICK_AXIS_Z_POS        = -44;
constexpr int KJ_JOYSTICK_AXIS_Z_NEG        = -45;
constexpr int KJ_JOYSTICK_ROTATION_Z_POS    = -46;
constexpr int KJ_JOYSTICK_ROTATION_Z_NEG    = -47;
constexpr int KJ_JOYSTICK_BUTTON_1  = -48;
constexpr int KJ_JOYSTICK_BUTTON_2  = -49;
constexpr int KJ_JOYSTICK_BUTTON_3  = -50;
constexpr int KJ_JOYSTICK_BUTTON_4  = -51;
constexpr int KJ_JOYSTICK_BUTTON_5  = -52;
constexpr int KJ_JOYSTICK_BUTTON_6  = -53;
constexpr int KJ_JOYSTICK_BUTTON_7  = -54;
constexpr int KJ_JOYSTICK_BUTTON_8  = -55;
constexpr int KJ_JOYSTICK_BUTTON_9  = -56;
constexpr int KJ_JOYSTICK_BUTTON_10 = -57;
constexpr int KJ_JOYSTICK_BUTTON_11 = -58;
constexpr int KJ_JOYSTICK_BUTTON_12 = -59;
constexpr int KJ_JOYSTICK_BUTTON_13 = -60;
constexpr int KJ_JOYSTICK_BUTTON_14 = -61;
constexpr int KJ_JOYSTICK_BUTTON_15 = -62;
constexpr int KJ_JOYSTICK_BUTTON_16 = -63;
constexpr int KJ_JOYSTICK_POV_UP         = -64;
constexpr int KJ_JOYSTICK_POV_UP_RIGHT   = -65;
constexpr int KJ_JOYSTICK_POV_RIGHT      = -66;
constexpr int KJ_JOYSTICK_POV_DOWN_RIGHT = -67;
constexpr int KJ_JOYSTICK_POV_DOWN       = -68;
constexpr int KJ_JOYSTICK_POV_DOWN_LEFT  = -69;
constexpr int KJ_JOYSTICK_POV_LEFT       = -70;
constexpr int KJ_JOYSTICK_POV_UP_LEFT    = -71;

// --- DualSense (Virtual controller) ----------------------------------------
constexpr int KJ_DUALSENSE_CROSS       = -80;
constexpr int KJ_DUALSENSE_CIRCLE      = -81;
constexpr int KJ_DUALSENSE_SQUARE      = -82;
constexpr int KJ_DUALSENSE_TRIANGLE    = -83;
constexpr int KJ_DUALSENSE_LB          = -84;
constexpr int KJ_DUALSENSE_RB          = -85;
constexpr int KJ_DUALSENSE_LS_CLICK    = -86;
constexpr int KJ_DUALSENSE_RS_CLICK    = -87;
constexpr int KJ_DUALSENSE_SHARE       = -88;
constexpr int KJ_DUALSENSE_OPTIONS     = -89;
constexpr int KJ_DUALSENSE_DPAD_UP     = -90;
constexpr int KJ_DUALSENSE_DPAD_DOWN   = -91;
constexpr int KJ_DUALSENSE_DPAD_LEFT   = -92;
constexpr int KJ_DUALSENSE_DPAD_RIGHT  = -93;
constexpr int KJ_DUALSENSE_LS_UP       = -94;
constexpr int KJ_DUALSENSE_LS_DOWN     = -95;
constexpr int KJ_DUALSENSE_LS_LEFT     = -96;
constexpr int KJ_DUALSENSE_LS_RIGHT    = -97;
constexpr int KJ_DUALSENSE_RS_UP       = -98;
constexpr int KJ_DUALSENSE_RS_DOWN     = -99;
constexpr int KJ_DUALSENSE_RS_LEFT     = -100;
constexpr int KJ_DUALSENSE_RS_RIGHT    = -101;
constexpr int KJ_DUALSENSE_LT          = -102;
constexpr int KJ_DUALSENSE_RT          = -103;

// --- Range / deadzone constants --------------------------------------------
// Key codes run from KJ_LAST_KEY (-103) to 255, so indices are offset by 103.
constexpr int KJ_LAST_KEY      = -103;
constexpr int KJ_KEY_OFFSET    = -KJ_LAST_KEY;         // 103
constexpr int KJ_KEY_ARRAY_SIZE = 256 + KJ_KEY_OFFSET; // 359

// Analogue stick deadzones, on the same -32767..32767 scale the sticks use. A
// mapped joystick axis key (KJ_JOYSTICK_AXIS_*) only reports as down once the
// stick is deflected further than its deadzone from neutral.
inline int KJ_AXIS_DEADZONE_X = 5000;
inline int KJ_AXIS_DEADZONE_Y = 5000;
inline int KJ_AXIS_DEADZONE_Z = 5000;
inline int KJ_AXIS_DEADZONE_R = 5000;

// Check whether a key currently has a mapping.
bool KJ_Controls_IsMapped(int key);

// Clear all physical key down states.
void KJ_ClearAllPhysicalDown();

// Called when a mapped callback throws, instead of letting std::terminate()
// unwind the polling thread.
using KJ_CallbackErrorHandler = void (*)(const char* callbackName, int key, const char* message);
void KJ_Controls_SetCallbackErrorHandler(KJ_CallbackErrorHandler handler);

// Control functions.
void KJ_Controls_Reset();
void KJ_Controls_Map(int key, void (*onPress)(), void (*onHeld)(), void (*onRelease)());
void KJ_Controls_NotifyWheelPulse(int key);
void KJ_Controls_SetMouseMoveCallback(void (*callback)(int, int));

// Device polling.
void Poll_Devices();
void Dispatch_Events();

// True while the background polling thread is running.
extern std::atomic<bool> KJ_InputThreadActive;

// Physical key down state (used by the keyboard/mouse hooks).
extern std::atomic<bool> keyPhysicalDown[256];