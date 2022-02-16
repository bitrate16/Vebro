#pragma once

#include "resource.h"
// Deprecated: #include "TrayMenuItems.h"

#include "WorkerWEnumerator.h"

#include "Strings.h"


// Link OpenGL
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

// Disable deprecation of freopen
#pragma warning(disable : 4996)

// Definitions
#define MAX_LOADSTRING 100
#define	WM_TRAY_ICON (WM_USER + 1)