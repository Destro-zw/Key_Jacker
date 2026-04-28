# Key_Jacker

An easy to use C++ input library for Windows that runs code on **press**, **hold** 
and **release** states, that will block/override the real key before it reaches
the target application. It can also be used in original applications as a key
input system. Currently used in Wing Loader as it's main input controller.
https://www.wcnews.com/chatzone/threads/wing-commander-1-ks-wcdx-loader-voice-over-mod.30387/post-415220

It provides low-level keyboard/mouse hooks, virtual input injection (keyboard,
mouse, Xbox controllers) and a legacy WinMM joystick hook that can present a 
virtual joystick to older games.

## Requirements

- Windows
- A C++17 compiler (tested with MSVC / Visual Studio)
- CMake 3.16+ to build

## Quick start

```cpp
#include "Key_Jacker.h"

// Pass the module handle and the window that must be focused for events to fire.
KJ_Install(hInstance, hWnd);

// Map a key: onPress fires once, onHold every loop, onRelease once.
KJ_Controls_Map(KJ_KEY_C,
    []{ /* pressed  */ },
    []{ /* held     */ },
    []{ /* released */ });

// When exiting application:
KJ_Uninstall();
```

## Key codes

See `src/KJ_Controls.h` for the full list of `KJ_KEY_*`, `KJ_MOUSE_*`,
`KJ_XBOX_*`, and `KJ_JOYSTICK_*` codes.

## Optional Functions
```cpp
bool KJ_Controls_IsMapped(int key);   // Returns true if that key is mapped.
void KJ_Controls_Reset();             // clear all mappings
```

## Building

To add Key_Jacker to your project, simpliy add the following to your CMakeLists.txt.
No download is required as the CMake script will fetch everything for you.
You can still download/clone Key_Jacker and modify it as needed, just make note on how
to add a local folder path to the CMake code below.

```cmake
include(FetchContent)

# Add local folder path inside "" if you manually downloaded or cloned Key_Jacker. 
# Otherwise it will fetch the repository automaticlly from github.
set(Key_Jacker_LOCAL_PATH "" CACHE PATH "Local Key_Jacker path")

if(Key_Jacker_LOCAL_PATH)
  FetchContent_Declare(Key_Jacker SOURCE_DIR ${Key_Jacker_LOCAL_PATH})
else()
  FetchContent_Declare(
    Key_Jacker
    GIT_REPOSITORY https://github.com/Destro-zw/Key_Jacker.git
    GIT_TAG        main
  )
endif()

FetchContent_MakeAvailable(Key_Jacker)

target_link_libraries(your_app PRIVATE Key_Jacker)
```
