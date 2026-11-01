/* Sits in front of DXVK's windows.h (which d3d9.h and the game include) and adds the Win32 API the game calls. */
#pragma once
#include_next <windows.h>
#ifdef __cplusplus
#include "win32_compat.h"
#endif
