// DesktopWallpaper.cpp : Defines the entry point for the application.
//
// Usable: https://zetcode.com/gui/winapi/menus/

#include "framework.h"
#include "DesktopWallpaper.h"


// Globals
// >> Tray related
NOTIFYICONDATA trayNID;
HWND trayWindow;
HICON trayIcon;

// >> Displays related
RECT fullViewportSize;         // Size of displays in total
std::vector<RECT> rawDisplays; // List of existing displays' dimensions as raw relative sizes
std::vector<RECT> displays;    // List of existing displays' dimensions

// >> Wallpaper related
RECT currentWindowDimensions;
HWND glWindow;
HPALETTE glPalette;
HDC glDevice;
HGLRC glContext;
int glWidth;
int glHeight;
BOOL glDebugOutput = FALSE;

// >> OpenGL related
GLuint glSCVBO, glSCVAO, glSCEBO; // Buffers for main scene square
// Define square vertices and indices for VBO
float glSCVertices[] = { 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f };
GLuint glSCIndices[] = { 0, 1, 2, 2, 3, 0 };

struct GLInput {
	// Determines type of the input
	char type;
	// Defines binding to the used texture / buffer
	GLuint bind = -1;
	// Defines additional data needed (For example audio track information / video information / e.t.c)
};

GLuint glMainShaderProgramID = -1;    // Main shader program ID
GLInput glMainInputs[4];              // > Inputs
GLuint glBufferAShaderProgramID = -1; // Buffer A shader program ID
GLInput glBufferAInputs[4];           // > Inputs
GLuint glBufferBShaderProgramID = -1; // Buffer B shader program ID
GLInput glBufferBInputs[4];           // > Inputs
GLuint glBufferCShaderProgramID = -1; // Buffer C shader program ID
GLInput glBufferCInputs[4];           // > Inputs
GLuint glBufferDShaderProgramID = -1; // Buffer D shader program ID
GLInput glBufferDInputs[4];           // > Inputs

// >> Scene related
BOOL   scFullscreen     = FALSE;      // Indicates if scene is fullscreen (Full desktop space)
BOOL   scPaused         = FALSE;      // Indicates if rendering is paused
double scPauseTimestamp = 0;          // Holds timestamp of the pause
double scTimestamp      = 0;          // Holds timestamp of the previous frame
int    scFrames         = 0;          // Holds number of frames rendered
BOOL   scMouseEnabled   = FALSE;      // Indicates if Mouse input is enabled
POINT  scMouse;                       // Holds last mouse location
double scMinFrameTime   = 1.0 / 30.0; // Holds minimal frame time (1.0 / FPS)
int    scFPSMode        = 30;         // Just defines the FPS used
BOOL   scSoundEnabled   = FALSE;      // Indicates if sound capture enabled / disabled. Used to force disable sound capture if shader uses audio input

// >> Threading related
std::thread renderThread;
std::mutex renderMutex;

// TODO:
// System sound source picker
// Audio input using FFT and texture
// Image textures
// Video textures
// Mouse clicks
// Buffer rendering
// Json-like format for storing shaders
// Direct download from shadertoy.com


// Initialize the OpenGL scene
void initSC() {
	// I forget what it does, so help me to render fragment shader only :D
	glViewport(0, 0, glWidth, glHeight);
	glClearColor(0, 0, 0, 0);

	glfwSetTime(0.0);
	scTimestamp = 0.0;
	scFrames = 0;

	glGenVertexArrays(1, &glSCVAO);
	glGenBuffers(1, &glSCVBO);
	glGenBuffers(1, &glSCEBO);

	glBindVertexArray(glSCVAO);

	glBindBuffer(GL_ARRAY_BUFFER, glSCVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glSCVertices), glSCVertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glSCEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(glSCIndices), glSCIndices, GL_STATIC_DRAW);

	glVertexAttribPointer(glGetAttribLocation(0, "position"), 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);
}

// Destroy Scene and all linked shaders / textures / resources
void destroySC() {
	if (glMainShaderProgramID != -1)
		glDeleteProgram(glMainShaderProgramID);
}

// Resize the OpenGL Scene
// values automatically calculated from [currentWindowDimensions]
void resizeSC() {
	glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
	glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

	glViewport(0, 0, glWidth, glHeight);
}

// Render single frame of the Scene
void renderSC() {
	if (glMainShaderProgramID != -1) {
		// TODO:
		// 1. Evaluate all inputs 
		// 2. Save buffers as textures
		// 3. Render all buffers
		// 4. Render scene

		if (!scPaused) {
			glClearColor(0, 0, 0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT);

			glUniform3f(glGetUniformLocation(glMainShaderProgramID, "iResolution"), (float)glWidth, (float)glHeight, 0.0);
			glUniform1f(glGetUniformLocation(glMainShaderProgramID, "iTime"), (float)glfwGetTime());
			glUniform1f(glGetUniformLocation(glMainShaderProgramID, "iTimeDelta"), (float)(glfwGetTime() - scTimestamp));
			glUniform1i(glGetUniformLocation(glMainShaderProgramID, "iFrame"), scFrames);

			if (scMouseEnabled) {
				// imouse.xy = current mouse location
				// iMouse.zw = previous mouse location (didn't figure out yet)
				POINT currentMouse;
				if (!GetCursorPos(&currentMouse))
					currentMouse = { 0, 0 };

				glUniform4f(glGetUniformLocation(glMainShaderProgramID, "iMouse"), (GLfloat)currentMouse.x, (GLfloat)currentMouse.y, (GLfloat)scMouse.x, (GLfloat)scMouse.y);

				scMouse.x = currentMouse.x;
				scMouse.y = currentMouse.y;
			} else
				glUniform4f(glGetUniformLocation(glMainShaderProgramID, "iMouse"), (GLfloat)scMouse.x, (GLfloat)scMouse.y, 0.0, 0.0);

			scTimestamp = (float)glfwGetTime();
			++scFrames;

			glUseProgram(glMainShaderProgramID);
			glBindVertexArray(glSCVAO);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);

			glFlush();
			SwapBuffers(glDevice);
		}
	}
}


// Starts thread for rendering scene, synchronizes with main thread for careful event processing and resource rebinding
void startRenderThread() {
	
}


// Carefull stop application with dispose of all resources, threads and exit
void exitApp() {

}


// Handler for enumerating all existing displays
BOOL CALLBACK displayEnumerateProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	MONITORINFO info;
	info.cbSize = sizeof(info);
	if (GetMonitorInfo(hMonitor, &info))
		rawDisplays.push_back(info.rcMonitor);

	return TRUE;
}

// Enumerates all existying displays in the system to save their size into global variables
void enumerateDisplays() {
	rawDisplays.clear();
	displays.clear();

	// Obsessive compulsive disorder wants me to zero it
	fullViewportSize.left = 0;
	fullViewportSize.right = 0;
	fullViewportSize.top = 0;
	fullViewportSize.bottom = 0;

	EnumDisplayMonitors(NULL, NULL, displayEnumerateProc, 0);

	// Find minimal coordinates
	int min_x = 0, min_y = 0;

	for (size_t i = 0; i < rawDisplays.size(); ++i) {
		fullViewportSize.left   = min(fullViewportSize.left,   rawDisplays[i].left);
		fullViewportSize.top    = min(fullViewportSize.top,    rawDisplays[i].top);
		fullViewportSize.right  = max(fullViewportSize.right,  rawDisplays[i].right);
		fullViewportSize.bottom = max(fullViewportSize.bottom, rawDisplays[i].bottom);

		min_x = min(rawDisplays[i].left, min_x);
		min_y = min(rawDisplays[i].top, min_y);
	}

	// Get resulting full viewport width & height
	fullViewportSize.right  -= fullViewportSize.left;
	fullViewportSize.bottom -= fullViewportSize.top;
	fullViewportSize.left = 0;
	fullViewportSize.top = 0;

	// Calculate absolute dimensions for displays
	displays.resize(rawDisplays.size());
	for (size_t i = 0; i < rawDisplays.size(); ++i) {
		int delta_x = rawDisplays[i].left - min_x;
		int delta_y = rawDisplays[i].top - min_y;

		displays[i].left   = rawDisplays[i].left   - min_x;
		displays[i].top    = rawDisplays[i].top    - min_y;
		displays[i].right  = rawDisplays[i].right  - min_x;
		displays[i].bottom = rawDisplays[i].bottom - min_y;
	}
}


// Tray window event dispatcher
LONG WINAPI trayWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { // TODO: Accelerators &
	// Main Tray menu
	HMENU trayMainMenu;
	// Tray menu for FPS selection
	HMENU trayFPSSelectMenu;
	// Tray menu sound source selection
	HMENU traySoundSourceMenu;
	// Tray menu Main shader config
	HMENU trayMainShaderMenu;
	// Tray menu buffer shader config
	HMENU trayMainBufferAShaderMenu;
	HMENU trayMainBufferBShaderMenu;
	HMENU trayMainBufferCShaderMenu;
	HMENU trayMainBufferDShaderMenu;
	// Tray menu input type selection
	HMENU trayMainInputTypeMenu;

	// Event specific info
	int wmId, wmEvent;

	// Tray click position
	POINT lpClickPoint;

	switch (uMsg) {
		case WM_TRAY_ICON: {
			switch (LOWORD(lParam)) {
				case WM_RBUTTONDOWN: {
					// Retrieve cursor position and spawn menu in click location
					GetCursorPos(&lpClickPoint);
					trayMainMenu = CreatePopupMenu();

					// Build level 0 menu
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_MOVETONEXTDISPLAY, _T("Move to next display"));
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_MOVETOPREVDISPLAY, _T("Move to prev display"));
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_FULLSCREEN, _T("Fullscreen"));
					if (scFullscreen)
						CheckMenuItem(trayMainMenu, ID_SYSTRAYMENU_FULLSCREEN, MF_CHECKED);
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_RESCAN_DISPLAYS, _T("Rescan displays"));

					//
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
					//

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_PAUSE, _T("Pause"));
					if (scPaused)
						CheckMenuItem(trayMainMenu, ID_SYSTRAYMENU_PAUSE, MF_CHECKED);
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_RESET_TIME, _T("Reset time"));
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_RELOAD_INPUTS, _T("Reload inputs"));
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_RELOAD_PACK, _T("Reload pack"));

					//
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
					//

					trayFPSSelectMenu = CreatePopupMenu();
					InsertMenu(trayFPSSelectMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_FPS_SUBMENU + 1, _T("1 FPS"));
					InsertMenu(trayFPSSelectMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_FPS_SUBMENU + 2, _T("15 FPS"));
					InsertMenu(trayFPSSelectMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_FPS_SUBMENU + 3, _T("30 FPS"));
					InsertMenu(trayFPSSelectMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_FPS_SUBMENU + 4, _T("60 FPS"));
					InsertMenu(trayFPSSelectMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_FPS_SUBMENU + 5, _T("120 FPS"));

					// IDK how to concat and use this LPWSTRPTR shit
					if (scFPSMode == 1) {
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayFPSSelectMenu, _T("FPS (1)"));
						CheckMenuItem(trayFPSSelectMenu, ID_SYSTRAYMENU_FPS_SUBMENU + 1, MF_CHECKED);
					} else if (scFPSMode == 15) {
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayFPSSelectMenu, _T("FPS (15)"));
						CheckMenuItem(trayFPSSelectMenu, ID_SYSTRAYMENU_FPS_SUBMENU + 2, MF_CHECKED);
					} else if (scFPSMode == 30) {
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayFPSSelectMenu, _T("FPS (30)"));
						CheckMenuItem(trayFPSSelectMenu, ID_SYSTRAYMENU_FPS_SUBMENU + 3, MF_CHECKED);
					} else if (scFPSMode == 60) {
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayFPSSelectMenu, _T("FPS (60)"));
						CheckMenuItem(trayFPSSelectMenu, ID_SYSTRAYMENU_FPS_SUBMENU + 4, MF_CHECKED);
					} else{ // scFPSMode == 120
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayFPSSelectMenu, _T("FPS (120)"));
						CheckMenuItem(trayFPSSelectMenu, ID_SYSTRAYMENU_FPS_SUBMENU + 5, MF_CHECKED);
					}

					//
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
					//

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_SOUND_MODE, _T("Enable sound capture"));
					if (scSoundEnabled)
						CheckMenuItem(trayMainMenu, ID_SYSTRAYMENU_SOUND_MODE, MF_CHECKED);

					// TODO: Enumerate sound inputs as submenu items + add selection mark
					traySoundSourceMenu = CreatePopupMenu();
					InsertMenu(traySoundSourceMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_SOUND_SOURCE + 1, _T("Low definition audio device"));
					InsertMenu(traySoundSourceMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_SOUND_SOURCE + 2, _T("Micwofone"));
					InsertMenu(traySoundSourceMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_SOUND_SOURCE + 3, _T("Moan box"));
					CheckMenuItem(traySoundSourceMenu, ID_SYSTRAYMENU_SOUND_SOURCE + 3, MF_CHECKED);

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) traySoundSourceMenu, _T("Sound source"));
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_MOUSE_MODE, _T("Enable mouse"));
					if (scMouseEnabled)
						CheckMenuItem(trayMainMenu, ID_SYSTRAYMENU_MOUSE_MODE, MF_CHECKED);

					//
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
					//

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_OPEN_PACK, _T("Open pack"));
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_SAVE_PACK, _T("Save pack"));
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_SAVE_PACK_AS, _T("Save pack as"));
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_NEW_PACK, _T("New pack"));

					//
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
					//

					trayMainInputTypeMenu = CreatePopupMenu();
					InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_INPUT_BUFFER_A, _T("Buffer A"));
					InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_INPUT_BUFFER_B, _T("Buffer B"));
					InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_INPUT_BUFFER_C, _T("Buffer C"));
					InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_INPUT_BUFFER_D, _T("Buffer D"));
					InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_INPUT_IMAGE, _T("Image"));
					InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_INPUT_MICROPHONE, _T("Microphone"));
					InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_INPUT_AUDIO, _T("Audio file"));
					InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_INPUT_VIDEO, _T("Video file"));
					InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_INPUT_WEBCAM, _T("Webcam"));

					trayMainShaderMenu = CreatePopupMenu();
					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_OPEN, _T("Open"));
					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_NEW, _T("New"));
					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 0"));
					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 1"));
					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 2"));
					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 3"));

					// TODO: Create new submenu for each buffer / shader and add short info about input type
					trayMainBufferAShaderMenu = CreatePopupMenu();
					InsertMenu(trayMainBufferAShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_OPEN, _T("Open"));
					InsertMenu(trayMainBufferAShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_NEW, _T("New"));
					InsertMenu(trayMainBufferAShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_REMOVE, _T("Remove"));
					InsertMenu(trayMainBufferAShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 0")); // TODO: Input preview
					InsertMenu(trayMainBufferAShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 1"));
					InsertMenu(trayMainBufferAShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 2"));
					InsertMenu(trayMainBufferAShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 3"));

					trayMainBufferBShaderMenu = CreatePopupMenu();
					InsertMenu(trayMainBufferBShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_OPEN, _T("Open"));
					InsertMenu(trayMainBufferBShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_NEW, _T("New"));
					InsertMenu(trayMainBufferBShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_REMOVE, _T("Remove"));
					InsertMenu(trayMainBufferBShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 0")); // TODO: Input preview
					InsertMenu(trayMainBufferBShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 1"));
					InsertMenu(trayMainBufferBShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 2"));
					InsertMenu(trayMainBufferBShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 3"));

					trayMainBufferCShaderMenu = CreatePopupMenu();
					InsertMenu(trayMainBufferCShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_OPEN, _T("Open"));
					InsertMenu(trayMainBufferCShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_NEW, _T("New"));
					InsertMenu(trayMainBufferCShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_REMOVE, _T("Remove"));
					InsertMenu(trayMainBufferCShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 0")); // TODO: Input preview
					InsertMenu(trayMainBufferCShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 1"));
					InsertMenu(trayMainBufferCShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 2"));
					InsertMenu(trayMainBufferCShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 3"));

					trayMainBufferDShaderMenu = CreatePopupMenu();
					InsertMenu(trayMainBufferDShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_OPEN, _T("Open"));
					InsertMenu(trayMainBufferDShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_NEW, _T("New"));
					InsertMenu(trayMainBufferDShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_REMOVE, _T("Remove"));
					InsertMenu(trayMainBufferDShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 0")); // TODO: Input preview
					InsertMenu(trayMainBufferDShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 1"));
					InsertMenu(trayMainBufferDShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 2"));
					InsertMenu(trayMainBufferDShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 3"));

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainShaderMenu, _T("Main shader"));
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainBufferAShaderMenu, _T("Buffer A shader"));
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainBufferBShaderMenu, _T("Buffer B shader"));
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainBufferCShaderMenu, _T("Buffer C shader"));
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainBufferDShaderMenu, _T("Buffer D shader"));

					//
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
					//

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_DEBUG_WARNINGS, _T("Enable debug output"));
					if (glDebugOutput)
						CheckMenuItem(trayMainMenu, ID_SYSTRAYMENU_DEBUG_WARNINGS, MF_CHECKED);
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_SYSTRAYMENU_EXIT, _T("Exit"));

					// Display menu, finally
					SetForegroundWindow(hWnd);
					TrackPopupMenu(trayMainMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN, lpClickPoint.x, lpClickPoint.y, 0, hWnd, NULL);
					return TRUE;
				}
				break;
			}
			break;
		}
		
		case WM_COMMAND: {
			wmId = LOWORD(wParam);
			wmEvent = HIWORD(wParam);

			switch (wmId) {
				case ID_SYSTRAYMENU_EXIT: {
					// TODO: Multithreaded exit
					Shell_NotifyIcon(NIM_DELETE, &trayNID);
					destroySC();
					DestroyWindow(hWnd);
					DestroyWindow(glWindow);
					break;
				}
				
				default:
					return DefWindowProc(hWnd, wmId, wParam, lParam);
			}
		}
		
		case WM_DESTROY: {
			PostQuitMessage(0);
			break;
		}
		
		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Initializes the tray menu window and related
// Returns 0 on success, 1 else
int createTrayMenu(_In_ HINSTANCE hInstance) {
	// Register window instance
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = trayWindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAY_ICON));
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_TRAY_ICON));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = L"MenuName";
	wcex.lpszClassName = L"SHADERWALLPAPER_Tray";

	if (!RegisterClassEx(&wcex)) {
		std::wcout << "Failed to register [SHADERWALLPAPER_TRAY] window class" << std::endl;
		return 1;
	}

	// Initialize window
	trayWindow = CreateWindow(L"SHADERWALLPAPER_Tray", L"Shader Wallpaper", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
	if (!trayWindow) {
		std::wcout << "Failed to create window with class [SHADERWALLPAPER_Tray]" << std::endl;
		return 1;
	}

	// Load icon
	trayIcon = LoadIcon(hInstance, (LPCTSTR) MAKEINTRESOURCE(IDI_TRAY_ICON));

	// Create Icon Data structure
	trayNID;
	trayNID.cbSize = sizeof(NOTIFYICONDATA);
	// Handle of the window which will process this app. messages 
	trayNID.hWnd = trayWindow;
	trayNID.uID = IDI_TRAY_ICON;
	trayNID.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	trayNID.hIcon = trayIcon;
	trayNID.uCallbackMessage = WM_TRAY_ICON;
	std::wcscpy(trayNID.szTip, L"Desktop Wallpaper Menu");

	// Push icon to tray
	Shell_NotifyIcon(NIM_ADD, &trayNID);

	return 0;
}


// Desktop window event dispatcher
LONG WINAPI desktopWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Initializes the GL window with context and cookies
// Returns 0 on success, 1 else
int createDesktopWindow(_In_ HINSTANCE hInstance) {
	// Locate WorkerW
	HWND workerw = WorkerWEnumerator::enumerateForWorkerW();

	if (workerw == NULL) {
		std::wcout << "WorkerW enumeration failed" << std::endl;
		return 1;
	}

	// Initial display enumeration
	enumerateDisplays();
	currentWindowDimensions = displays[0];

	// Creating Window for OpenGL context
	WNDCLASS wc;
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC) desktopWindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"DESKTOPWALLPAPER_OpenGL";

	if (!RegisterClass(&wc)) {
		std::wcout << "Failed to register [DESKTOPWALLPAPER_OpenGL] window class" << std::endl;
		return 1;
	}

	// Initialize window
	glWindow = CreateWindow(L"DESKTOPWALLPAPER_OpenGL", L"Sample Text", WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, NULL, NULL, hInstance, NULL);
	
	if (!glWindow) {
		std::wcout << "Failed to create window with class [DESKTOPWALLPAPER_OpenGL]" << std::endl;
		return 1;
	}

	// Change window style to remove border nand decorations
	DWORD style = ::GetWindowLong(glWindow, GWL_STYLE);
	style &= ~WS_OVERLAPPEDWINDOW;
	style |= WS_POPUP;
	::SetWindowLong(glWindow, GWL_STYLE, style);

	glDevice = GetDC(glWindow);

	// Apply pixel format for the window
	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd, 0, sizeof(pfd));
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;

	int pf = ChoosePixelFormat(glDevice, &pfd);
	if (pf == 0) {
		std::wcout << "Can not find a suitable pixel format" << std::endl;
		return 1;
	}

	if (SetPixelFormat(glDevice, pf, &pfd) == FALSE) {
		std::wcout << "Can not set specified pixel format" << std::endl;
		return 1;
	}

	DescribePixelFormat(glDevice, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

	if (pfd.dwFlags & PFD_NEED_PALETTE || pfd.iPixelType == PFD_TYPE_COLORINDEX) {
		int n = 1 << pfd.cColorBits;
		if (n > 256) 
			n = 256;

		LOGPALETTE* lpPal = (LOGPALETTE*) malloc(sizeof(LOGPALETTE) + sizeof(PALETTEENTRY) * n);
		if (!lpPal) {
			std::wcout << "Failed to allocate palette" << std::endl;
			return 1;
		}

		memset(lpPal, 0, sizeof(LOGPALETTE) + sizeof(PALETTEENTRY) * n);
		lpPal->palVersion = 0x300;
		lpPal->palNumEntries = n;

		GetSystemPaletteEntries(glDevice, 0, n, &lpPal->palPalEntry[0]);

		if (pfd.iPixelType == PFD_TYPE_RGBA) {
			int redMask   = (1 << pfd.cRedBits)   - 1;
			int greenMask = (1 << pfd.cGreenBits) - 1;
			int blueMask  = (1 << pfd.cBlueBits)  - 1;
			int i;

			for (i = 0; i < n; ++i) {
				lpPal->palPalEntry[i].peRed   = (((i >> pfd.cRedShift) & redMask) * 255) / redMask;
				lpPal->palPalEntry[i].peGreen = (((i >> pfd.cGreenShift) & greenMask) * 255) / greenMask;
				lpPal->palPalEntry[i].peBlue  = (((i >> pfd.cBlueShift) & blueMask) * 255) / blueMask;
				lpPal->palPalEntry[i].peFlags = 0;
			}
		}
		else {
			lpPal->palPalEntry[0].peRed   = 0;
			lpPal->palPalEntry[0].peGreen = 0;
			lpPal->palPalEntry[0].peBlue  = 0;
			lpPal->palPalEntry[0].peFlags = PC_NOCOLLAPSE;
			lpPal->palPalEntry[1].peRed   = 255;
			lpPal->palPalEntry[1].peGreen = 0;
			lpPal->palPalEntry[1].peBlue  = 0;
			lpPal->palPalEntry[1].peFlags = PC_NOCOLLAPSE;
			lpPal->palPalEntry[2].peRed   = 0;
			lpPal->palPalEntry[2].peGreen = 255;
			lpPal->palPalEntry[2].peBlue  = 0;
			lpPal->palPalEntry[2].peFlags = PC_NOCOLLAPSE;
			lpPal->palPalEntry[3].peRed   = 0;
			lpPal->palPalEntry[3].peGreen = 0;
			lpPal->palPalEntry[3].peBlue  = 255;
			lpPal->palPalEntry[3].peFlags = PC_NOCOLLAPSE;
		}

		glPalette = CreatePalette(lpPal);
		if (glPalette) {
			SelectPalette(glDevice, glPalette, FALSE);
			RealizePalette(glDevice);
		}

		free(lpPal);
	}

	// Create GL context & init
	glContext = wglCreateContext(glDevice);
	wglMakeCurrent(glDevice, glContext);

	if (!glfwInit()) {
		std::wcout << "Failed to initialize GLFW" << std::endl;
		return 1;
	}

	glewExperimental = GL_TRUE;
	if (GLEW_OK != glewInit()) {
		std::wcout << "Failed to initialize GLEW" << std::endl;
		return 1;
	}

	glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
	glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

	// Pass window to the WorkerW
	ReleaseDC(glWindow, glDevice);
	SetParent(glWindow, workerw);

	return 0;
}


// Message dispatcher simply handles all WINAPI messages and do it's work
void enterDispatchLoop() {
	// TODO: Multithreaded render to avoid image freeze

	std::chrono::system_clock::time_point a = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point b = std::chrono::system_clock::now();

	MSG msg;

	while (1) {
		if (!scPaused) {
			a = std::chrono::system_clock::now();
			std::chrono::duration<double, std::milli> work_time = a - b;

			while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
				if (GetMessage(&msg, NULL, 0, 0)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				} else
					goto loopExit;
			}

			if (msg.message == WM_QUIT)
				goto loopExit;

			// One frame render
			renderSC();

			if (work_time.count() < scMinFrameTime) {
				std::chrono::duration<double, std::milli> delta_ms(scMinFrameTime - work_time.count());
				auto delta_ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
				std::this_thread::sleep_for(std::chrono::milliseconds(delta_ms_duration.count()));
			}

			b = std::chrono::system_clock::now();
		} else {
			// Block till message received
			if (GetMessage(&msg, NULL, 0, 0)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			} else
				goto loopExit;
		}
	}

loopExit:
	return;
}


// Entry
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow) {

	//UNREFERENCED_PARAMETER(hPrevInstance);
	//UNREFERENCED_PARAMETER(pCmdLine);

	// Debug: Display console window
#ifdef DEBUG_DISPLAY_CONSOLE_WINDOW
	AllocConsole();
	FILE* reo = freopen("CONOUT$", "w+", stdout);
#endif

	// Change output mode to Unicode text
	int _sm = _setmode(_fileno(stdout), _O_U16TEXT);

	// Create Tray menu
	if (createTrayMenu(hInstance)) {
		std::wcout << "Failed initialization of Tray Menu window" << std::endl;
		system("PAUSE");
		return 0;
	}

	// Create window on desktop for wallpaper
	if (createDesktopWindow(hInstance)) {
		std::wcout << "Failed initialization of GL context or Worker window" << std::endl;
		system("PAUSE");
		return 0;
	}

	// Initialize render context
	initSC();
	ShowWindow(glWindow, nCmdShow);
	UpdateWindow(glWindow);

	//TODO: Start new thread for render
	startRenderThread();

	// Enter message dispatching loop
	enterDispatchLoop();

	// After dispatch loop: exit and dispose
	destroySC();
	wglMakeCurrent(NULL, NULL);
	ReleaseDC(glWindow, glDevice);
	wglDeleteContext(glContext);
	DestroyWindow(glWindow);

	if (glPalette)
		DeleteObject(glPalette);

	return 0;
}

