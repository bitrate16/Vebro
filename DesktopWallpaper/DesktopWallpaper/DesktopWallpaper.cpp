// DesktopWallpaper.cpp : Defines the entry point for the application.
//
// Usable: https://zetcode.com/gui/winapi/menus/

#include "framework.h"
#include "DesktopWallpaper.h"


// Globals
// >> Tray related
NOTIFYICONDATA trayNID;
HWND           trayWindow;
HICON          trayIcon;

// >> Displays related
RECT              fullViewportSize; // Size of displays in total
std::vector<RECT> rawDisplays;      // List of existing displays' dimensions as raw relative sizes
std::vector<RECT> displays;         // List of existing displays' dimensions

// >> Wallpaper related
RECT     currentWindowDimensions;
HWND     glWindow;
HPALETTE glPalette;
HDC      glDevice;
HGLRC    glContext;
int      glWidth;
int      glHeight;
BOOL     glDebugOutput = FALSE;

// >> OpenGL related
// Buffers for viewport coords
GLuint glSquareVAO;
GLuint glSquareVBO;
GLuint glSquareEBO;

// Shaders (if exists)
GLuint glMainShaderProgramID = -1;   // Main shader program ID
const char* glMainShaderPath = NULL; // Path to the main shader (For support reload button)

// Value == -1 indicates that shader sould not be rendered
GLuint glBufferShaderProgramIDs[4] = { -1, -1, -1, -1 };        // Buffer i shader program (A / B / C / D)
const char* glBufferShaderPath[4] = { NULL, NULL, NULL, NULL }; // Path to the Buffer i shader (For support reload button)

// Framebuffers for these shaders
GLuint glBufferShaderFramebuffers[4];

// Textures to render in.
//  This textures will exist till the end of the program because they 
//   are used as temp variables storing buffer output for the next frame 
//   and can be used as input even if shader for this buffer (A / B / C / D) 
//   was removed.
GLuint glBufferShaderFramebufferTextures[4];

// Indicates if buffer[i] should be rendered
// Changed view resource control
// Render requires (glBufferShaderShouldBeRendered[i] && glBufferShaderProgramIDs[i] != -1)
//  to be true in order to render the shader because shader can be 
//  loaded but not rendered (in this case it is only compiled and not used) or 
//  rendered but not loaded (in this case buffer texture of shader is rendered, but code is not compiled).
// Shader code can be unloaded by Remove button in menu.
// Shader as input can be disabled by removing it manually from inputs.
BOOL glBufferShaderShouldBeRendered[4] = { FALSE, FALSE, FALSE, FALSE };


// >> Scene related
BOOL   scFullscreen     = FALSE;       // Indicates if scene is fullscreen (Full desktop space)
int    scDisplayID      = 0;           // Contains ID of the display that is currently used as render display 
BOOL   scPaused         = FALSE;       // Indicates if rendering is paused
double scPauseTimestamp = 0;           // Holds timestamp of the pause
double scTimestamp      = 0;           // Holds timestamp of the previous frame
int    scFrames         = 0;           // Holds number of frames rendered
BOOL   scMouseEnabled   = FALSE;       // Indicates if Mouse input is enabled
POINT  scMouse = { 0, 0 };             // Holds last mouse location
int    scMinFrameTime   = 1000 / 30;   // Holds minimal frame time in ms (1000 / FPS)
int    scFPSMode        = 30;          // Just defines the FPS used
BOOL   scSoundEnabled   = FALSE;       // Indicates if sound capture enabled / disabled. Used to force disable sound capture if shader uses audio input

// ID's for all shader inputs
// Should only be changed via special functions to correctly process GC
int scMainShaderInputs[4] = { -1, -1, -1, -1 };
int scBufferShaderInputs[4][4] = { { -1, -1, -1, -1 }, { -1, -1, -1, -1 }, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } };


// >> Threading related
std::thread*                          renderThread = nullptr;   // Thread for rendering the wallpaper
std::mutex                            renderMutex;              // Captured on each draw frame
BOOL                                  appExiting = FALSE;       // Indicates if application exits
BOOL                                  appLockRequested = FALSE; // indicates if main thread wants to acquire lock


// >> Resources related
// Resource type
// Buffer : id (A / B / C / D)
// Image : path, GLuint bind (image texture)
// Video : path, GLuint bind (current frame as texture), specific media data
// Audio : path, GLuint bind (current FFT as texture), specific media data
// Webcam : GLuint bind (current frame as texture)
// Microphone : GLuint (current FFT as texture)
// TODO: Keyboard Input, Cubemap input
enum ResourceType {
	IMAGE_TEXTURE, // Simple image as input
	AUDIO_TEXTURE, // Audio file as input
	VIDEO_TEXTURE, // Video file as input
	MIC_TEXTURE,   // Microphone as inout
	WEB_TEXTURE,   // Webcam as input
	FRAME_BUFFER   // Buffer A / B / C / D as input
};

// Resource unit
struct SCResource {

	// Resource references amount (auto GC)
	long refs = 0;

	// Resource type
	ResourceType type;

	// Bind for texture (id exists for this unit)
	GLuint bind = 0;

	// ID number of buffer (only for buffer input)
	char buffer_id = 0;

	// Absolute path of resource (used to determine duplications)
	char* path;
};

// Resource storage table (used to prevent reduplications of loading and processing)
// Storas static fixed amount of resources matching the total amount of inputs for all shaders
// Allows deletion of any element and insertion into any free cell
struct ResourceTableEntry {
	BOOL empty = TRUE;
	SCResource resource;
};

// 4 main inputs + 4 buffers * 4 inputs
const int ResourceTableSize = 4 + 4 * 4;

ResourceTableEntry scResources[ResourceTableSize];

// Finds the specified resource and returns it's ID on success
int findResource(SCResource res) {
	for (size_t i = 0; i < ResourceTableSize; ++i) {
		if (scResources[i].empty)
			continue;

		if (res.type == scResources[i].resource.type) {
			switch (res.type) {
				case IMAGE_TEXTURE: {
					if (strcmp(res.path, scResources[i].resource.path) == 0)
						return i;
					return -1;
				}

				case AUDIO_TEXTURE: {
					if (strcmp(res.path, scResources[i].resource.path) == 0)
						return i;
					return -1;
				}

				case VIDEO_TEXTURE: {
					if (strcmp(res.path, scResources[i].resource.path) == 0)
						return i;
					return -1;
				}

				case MIC_TEXTURE:
				case WEB_TEXTURE: {
					return -1;
				}

				case FRAME_BUFFER: {
					if (res.buffer_id == scResources[i].resource.buffer_id)
						return i;
					return -1;
				}
			}
		}
	}

	return -1;
}

// Load and initialize the desired resource
// Returns ResourceID as index in scResources or -1
int loadResource(SCResource res) {

	int resID = findResource(res);

	if (resID != -1) {
		++scResources[resID].resource.refs;
		return resID;
	}

	// Called once when initially loading resource:
	switch (res.type) {
		case IMAGE_TEXTURE: { // TODO: Load media resources
			return -1;
		}

		case AUDIO_TEXTURE: {
			return -1;
		}

		case VIDEO_TEXTURE: {
			return -1;
		}

		case MIC_TEXTURE: {
			return -1;
		}

		case WEB_TEXTURE: {
			return -1;
		}

		case FRAME_BUFFER: {
			++res.refs;

			glBufferShaderShouldBeRendered[res.buffer_id] = TRUE;

			// Insert into first free cell
			for (int i = 0; i < ResourceTableSize; ++i)
				if (scResources[i].empty) {
					scResources[i].empty = FALSE;
					scResources[i].resource = res;
					return i;
				}

			std::wcout << "Can not insert resource, resource table is corrupted" << std::endl;
			return -1;
		}
	}

	return -1;
}

// Unloads the resource with given ID
// If resource is used somewhere, it's refs count decreased, else it is completely disposed.
void unloadResource(int resID) {
	
	if (scResources[resID].empty) {
		std::wcout << "Can not unload empty resource, resource table is corrupted" << std::endl;
		return;
	}

	// Check if references > 0
	if (scResources[resID].resource.refs > 1) {
		--scResources[resID].resource.refs;
		return;
	}

	if (scResources[resID].resource.refs <= 0) {
		std::wcout << "Can not unload unreferenced resource, resource table is corrupted" << std::endl;
		return;
	}
	
	// In other case unload the resource
	scResources[resID].empty = TRUE;

	// Called once when finally disposing resource:
	switch (scResources[resID].resource.type) {
		case IMAGE_TEXTURE: { // TODO: Unload media resources
			return;
		}

		case AUDIO_TEXTURE: {
			return;
		}

		case VIDEO_TEXTURE: {
			return;
		}

		case MIC_TEXTURE: {
			return;
		}

		case WEB_TEXTURE: {
			return;
		}

		case FRAME_BUFFER: {
			glBufferShaderShouldBeRendered[scResources[resID].resource.buffer_id] = FALSE;
			return;
		}
	}
}

// Performs reloading of all resources
// Returns 0 on success, 1 else
BOOL reloadResources() {
	for (size_t i = 0; i < ResourceTableSize; ++i) {
		if (scResources[i].empty)
			continue;

		switch (scResources[i].resource.type) {
			case IMAGE_TEXTURE: { // TODO: Reload media resources
				break;
			}

			case AUDIO_TEXTURE: {
				break;
			}

			case VIDEO_TEXTURE: {
				break;
			}

			case MIC_TEXTURE: {
				break;
			}

			case WEB_TEXTURE: {
				break;
			}

			case FRAME_BUFFER: {
				break;
			}
		}
	}

	return 0;
}

// Unloads all resources
void unloadResources() {
	for (size_t i = 0; i < ResourceTableSize; ++i) {
		if (scResources[i].empty)
			continue;

		switch (scResources[i].resource.type) {
			case IMAGE_TEXTURE: { // TODO: Unload media resources
				break;
			}

			case AUDIO_TEXTURE: {
				break;
			}

			case VIDEO_TEXTURE: {
				break;
			}

			case MIC_TEXTURE: {
				break;
			}

			case WEB_TEXTURE: {
				break;
			}

			case FRAME_BUFFER: {
				break;
			}
		}

		scResources[i].empty = TRUE;
	}
}

// Performs load of main shader resource
// Warning: Path should be absolute
// Returns 0 on success, 1 else
BOOL loadMainShaderResource(SCResource res, int inputID) {

	// If main input is bound to some resource, reload this resource
	// Optionally check if target resource is loaded in this input and cancel load
	if (scMainShaderInputs[inputID] != -1) {

		int resID = findResource(res);

		// Do not relod the resource
		if (resID == scMainShaderInputs[inputID])
			return 0;

		// If resource does not exist or is not bound to this input, unload it
		unloadResource(scMainShaderInputs[inputID]);
		scMainShaderInputs[inputID] = -1;
	}

	scMainShaderInputs[inputID] = loadResource(res);
	return scMainShaderInputs[inputID] == -1;
}

// Performs load of main shader resource
// Warning: Path should be absolute
// Returns 0 on success, 1 else
BOOL loadBufferShaderResource(SCResource res, int bufferID, int inputID) {

	if (bufferID < 0 || bufferID > 3) {
		std::wcout << "Can not load resource for Buffer " << bufferID << ", Buffer does not exist, bufferID corrupt" << std::endl;
		return 1;
	}

	// If main input is bound to some resource, reload this resource
	// Optionally check if target resource is loaded in this input and cancel load
	if (scBufferShaderInputs[bufferID][inputID] != -1) {

		int resID = findResource(res);

		// Do not relod the resource
		if (resID == scBufferShaderInputs[bufferID][inputID])
			return 0;

		// If resource does not exist or is not bound to this input, unload it
		unloadResource(scBufferShaderInputs[bufferID][inputID]);
		scBufferShaderInputs[bufferID][inputID] = -1;
	}

	scBufferShaderInputs[bufferID][inputID] = loadResource(res);
	return scBufferShaderInputs[bufferID][inputID] == -1;
}


// TODO:
// System sound source picker
// Audio input using FFT and texture
// Image textures
// Video textures
// Mouse clicks
// Buffer rendering
// Json-like format for storing shaders
// Direct download from shadertoy.com


struct ShaderCompilationStatus {
	GLuint shaderID = -1;
	BOOL success = FALSE;
};

// Compiles fragment shader and returns shader program ID
// Debug only
ShaderCompilationStatus compileShader(const char* fragmentSource) {

	// Default Vertex shader
	const char* vertexSource = R"glsl(
		#version 330 core

		in vec2 position;

		void main()
		{
			gl_Position = vec4(position, 0.0, 1.0);
		}
	)glsl";

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);

	GLint status;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);

	if (glDebugOutput && status != GL_TRUE) {

		// TODO: std::vector<char> or std::string buffer
		char buffer[4096];
		glGetShaderInfoLog(vertexShader, 4096, NULL, buffer);

		MessageBoxA(
			NULL,
			buffer,
			"Vertex shader compilation error",
			MB_ICONERROR | MB_OK
		);

		return { 0, FALSE };
	}

	// Compile Fragment shader
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);

	status;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);

	if (glDebugOutput && status != GL_TRUE) {

		// TODO: std::vector<char> or std::string buffer
		char buffer[4096];
		glGetShaderInfoLog(fragmentShader, 4096, NULL, buffer);

		MessageBoxA(
			NULL,
			buffer,
			"Fragment shader compilation error",
			MB_ICONERROR | MB_OK
		);

		return { 0, FALSE };
	}

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);


	return { shaderProgram, TRUE };
}

// Load shader and then compile
ShaderCompilationStatus compileShaderFromFile(const char* path) {
	std::ifstream f(path);
	std::string str;

	if (!f) {
		MessageBoxA(
			NULL,
			(std::string("Failed to load file ") + path).c_str() ,
			"Failed to load file",
			MB_ICONERROR | MB_OK
		);

		return { 0, FALSE };
	}

	f.seekg(0, std::ios::end);
	str.reserve(f.tellg());
	f.seekg(0, std::ios::beg);

	str.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

	return compileShader(str.c_str());
}

// Loads Main shader from file, saves path
// Returns 0 on success, 1 else
BOOL loadMainShaderFromFile(const char* path) {
	
	// Set path for main shader
	glMainShaderPath = path;
	
	// Delete previous shader program
	if (glMainShaderProgramID != -1)
		glDeleteProgram(glMainShaderProgramID);

	glMainShaderProgramID = -1;

	ShaderCompilationStatus shaderResult = compileShaderFromFile(glMainShaderPath);
	if (shaderResult.success) {
		glMainShaderProgramID = shaderResult.shaderID;
		return 0;
	}

	return 1;
}

// Reloads Main shader from saved path
// Returns 0 on success, 1 else
BOOL reloadMainShader() {

	if (glMainShaderProgramID == -1) { // Not loaded, ignore
		return 0;
	} else if (glMainShaderPath == NULL) { // Loaded, but no path -> corrupt
		std::wcout << "Can not load Main shader, Shader path corrupt" << std::endl;
		return 1;
	}

	// Delete previous shader program
	if (glMainShaderProgramID != -1)
		glDeleteProgram(glMainShaderProgramID);

	glMainShaderProgramID = -1;

	ShaderCompilationStatus shaderResult = compileShaderFromFile(glMainShaderPath);
	if (shaderResult.success) {
		glMainShaderProgramID = shaderResult.shaderID;
		return 0;
	}

	return 1;
}

// Unloads Main shader from saved path
void unloadMainShader() {
	if (glMainShaderProgramID != -1)
		glDeleteProgram(glMainShaderProgramID);

	glMainShaderPath = NULL;
}

// Loads Buffer i shader from file, saves path
// Returns 0 on success, 1 else
BOOL loadBufferShaderFromFile(const char* path, int buffer_id) {

	if (buffer_id < 0 || buffer_id > 3) {
		std::wcout << "Can not load Shader for Buffer " << buffer_id << ", Buffer does not exist, buffer_id corrupt" << std::endl;
		return 1;
	}

	// Set path for main shader
	glBufferShaderPath[buffer_id] = path;

	// Delete previous shader program
	if (glBufferShaderProgramIDs[buffer_id] != -1)
		glDeleteProgram(glBufferShaderProgramIDs[buffer_id]);

	glBufferShaderProgramIDs[buffer_id] = -1;

	ShaderCompilationStatus shaderResult = compileShaderFromFile(glBufferShaderPath[buffer_id]);
	if (shaderResult.success) {
		glBufferShaderProgramIDs[buffer_id] = shaderResult.shaderID;
		return 0;
	}

	return 1;
}

// Loads Buffer i shader from saved path
// Returns 0 on success, 1 else
BOOL reloadBufferShaderFromFile(int buffer_id) {

	if (buffer_id < 0 || buffer_id > 3) {
		std::wcout << "Can not reload Shader for Buffer " << buffer_id << ", Buffer does not exist, buffer_id corrupt" << std::endl;
		return 1;
	}

	if (glBufferShaderProgramIDs[buffer_id] == -1) { // Not loaded, ignore
		return 0;
	} else if (glBufferShaderPath[buffer_id] == NULL) { // Loaded, but no path -> corrupt
		std::wcout << "Can not reload Shader for Buffer " << ("ABCD"[buffer_id]) << ", Shader path corrupt" << std::endl;
		return 1;
	}

	// Delete previous shader program
	if (glBufferShaderProgramIDs[buffer_id] != -1)
		glDeleteProgram(glBufferShaderProgramIDs[buffer_id]);

	glBufferShaderProgramIDs[buffer_id] = -1;

	ShaderCompilationStatus shaderResult = compileShaderFromFile(glBufferShaderPath[buffer_id]);
	if (shaderResult.success) {
		glBufferShaderProgramIDs[buffer_id] = shaderResult.shaderID;
		return 0;
	}

	return 1;
}

// Unloads Main shader from saved path
void unloadBufferShader(int buffer_id) {
	if (glBufferShaderProgramIDs[buffer_id] != -1)
		glDeleteProgram(glBufferShaderProgramIDs[buffer_id]);

	glBufferShaderPath[buffer_id] = NULL;
}


// Initialize the OpenGL scene
void initSC() {

	// Here be dragons
	glViewport(0, 0, glWidth, glHeight);
	glClearColor(0, 0, 0, 0);

	glfwSetTime(0.0);
	scTimestamp = 0.0;
	scFrames = 0;

	// Reference for stupid me: https://open.gl/drawing

	// Generate Array buffer with vertices of square
	const float squareVertices[] = {
		 1.0f,  1.0f,
		 1.0f, -1.0f,
		-1.0f, -1.0f,
		-1.0f,  1.0f
	};

	const GLuint squareIndices[] = {
		0, 1, 2,
		2, 3, 0
	};

	glGenVertexArrays(1, &glSquareVAO);
	glGenBuffers(1, &glSquareVBO);
	glGenBuffers(1, &glSquareEBO);

	glBindVertexArray(glSquareVAO);

	glBindBuffer(GL_ARRAY_BUFFER, glSquareVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(squareVertices), squareVertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glSquareEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(squareIndices), squareIndices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*) 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);


	// Create buffer and texture for all buffers (4 in total)
	glGenFramebuffers(4, glBufferShaderFramebuffers);
	glGenTextures(4, glBufferShaderFramebufferTextures);

	for (int i = 0; i < 4; ++i) {
		glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[i]);
		glViewport(0, 0, glWidth, glHeight);

		glBindTexture(GL_TEXTURE_2D, glBufferShaderFramebufferTextures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glWidth, glHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, glBufferShaderFramebufferTextures[i], 0);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

// Resize the OpenGL Scene
// values automatically calculated from [currentWindowDimensions]
void resizeSC() {
	glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
	glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

	glViewport(0, 0, glWidth, glHeight);

	// Resize all buffer textures
	for (int i = 0; i < 4; ++i) {
		glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[i]);
		glViewport(0, 0, glWidth, glHeight);

		glBindTexture(GL_TEXTURE_2D, glBufferShaderFramebufferTextures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glWidth, glHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
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

			// Each shader has following inputs (From shadertoy.com):
			// 
			// Basic:
			// uniform vec3      iResolution;           // viewport resolution (in pixels)
			// uniform float     iTime;                 // shader playback time (in seconds)
			// uniform float     iTimeDelta;            // render time (in seconds)
			// uniform int       iFrame;                // shader playback frame
			// uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
			// uniform vec4      iDate;                 // (year, month, day, time in seconds)
			// 
			// Advanced:
			// uniform float     iChannelTime[4];       // channel playback time (in seconds)
			// uniform vec3      iChannelResolution[4]; // channel resolution (in pixels)
			// uniform samplerXX iChannel0..3;          // input channel. XX = 2D/Cube
			// 
			// TODO: Deprecated (Always 0 yet):
			// uniform float     iSampleRate;           // sound sample rate (i.e., 44100)

			// Basic values:
			time_t t = std::time(NULL);
			tm* timePtr = std::localtime(&t);
			// Warning: If you are from 16,777,215 earth year, this programm will stop normally function in the next year
			//          If you are not from earth or do not rely on gregorian calendar, ignore this message
			//          Si per accidens, vocavit ab exterioris ratio in usu hoc progressio, vos can contact bitrate16@gmail.com 
			//           ad tollendam hac, et revertere ad pristina circulus inferni.
			//          In case of containment breach caused by this program, activate protocol SCP-[DELETED]-Alpha to avoid [DATA CORRUPTED].
			//          R WAAQ COS DaR AMITo 7 > DOS mot
			// TODO: tuturudu
			float iDate_year = (float) timePtr->tm_year;
			float iDate_month = (float) timePtr->tm_mon;
			float iDate_day = (float) timePtr->tm_mday;
			float iDate_time = (float) (timePtr->tm_hour * 60 * 60 + timePtr->tm_min * 60 + timePtr->tm_sec);

			// Calculate mouse location (if enabled)
			POINT currentMouse = { 0, 0 };

			if (scMouseEnabled) {
				if (!GetCursorPos(&currentMouse))
					currentMouse = { 0, 0 };
				else {
					// Compute mouse relative to each display in case of single screen
					currentMouse.x -= currentWindowDimensions.left;
					currentMouse.y -= currentWindowDimensions.top;
					currentMouse.y = currentWindowDimensions.bottom + currentWindowDimensions.top - currentMouse.y;
				}
			}

			// TODO: Move following code out of renderSC to prevent resource evaluation count in scene render time
			// TODO: Evaluate dynamic resources (Video / webcam frames, audio / microphone FFT's)
			for (size_t i = 0; i < ResourceTableSize; ++i) {
				if (scResources[i].empty)
					continue;

				switch (scResources[i].resource.type) {
					case AUDIO_TEXTURE: { // TODO: Evaluate dynamic resources media resources
						break;
					}

					case VIDEO_TEXTURE: {
						break;
					}

					case MIC_TEXTURE: {
						break;
					}

					case WEB_TEXTURE: {
						break;
					}
				}
			}

			// Evaluate buffers
			// Constant names for optimize speed:
			const char* const iChannelResolutionUniforms[4] = {
				"iChannelResolution[0]",
				"iChannelResolution[1]",
				"iChannelResolution[2]",
				"iChannelResolution[3]"
			};

			const char* const iChannelTimeUniforms[4] = {
				"iChannelTime[0]",
				"iChannelTime[1]",
				"iChannelTime[2]",
				"iChannelTime[3]"
			};

			const char* const iChannelUniforms[4] = {
				"iChannel0",
				"iChannel1",
				"iChannel2",
				"iChannel3"
			};

			// Render all buffers
			// TODO: Asynchronous buffer & main shader rendering
			for (int i = 0; i < 4; ++i) {

				// Require both conditions to complete in order to render
				if (glBufferShaderShouldBeRendered[i] && glBufferShaderProgramIDs[i] != -1) {

					// TODO: Should copy rexture first?
					glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[i]);
					glClearColor(0, 0, 0, 0);
					glClear(GL_COLOR_BUFFER_BIT);

					// Load all Basic inputs
					glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iResolution"), (float) glWidth, (float) glHeight, 0.0);
					glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iTime"), (float) glfwGetTime());
					glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iTimeDelta"), (float) (glfwGetTime() - scTimestamp));
					glUniform1i(glGetUniformLocation(glBufferShaderProgramIDs[i], "iFrame"), scFrames);

					// imouse.xy = current mouse location
					// iMouse.zw = previous mouse location
					// TODO: Validate iMouse.zw data
					if (scMouseEnabled) 
						glUniform4f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iMouse"), (GLfloat) currentMouse.x, (GLfloat) currentMouse.y, (GLfloat) scMouse.x, (GLfloat) scMouse.y);
					else
						glUniform4f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iMouse"), (GLfloat) scMouse.x, (GLfloat) scMouse.y, 0, 0);

					glUniform4f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iDate"), (GLfloat) iDate_year, (GLfloat) iDate_month, (GLfloat) iDate_day, (GLfloat) iDate_time);

					// Default value for SampleRate
					// TODO: Should evaluate from inputs?
					glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iSampleRate"), 0);

					// Bind iChannel data
					for (int k = 0; k < 4; ++k) {
						if (scBufferShaderInputs[i][k] == -1) {
							glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
							glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
							continue;
						}

						if (scResources[scBufferShaderInputs[i][k]].empty) {
							std::wcout << "Can not configure iChannelResolution for input " << k << " in Buffer " << i << ", input points to empty resource, scResources corrupt" << std::endl;
							glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
							glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
							continue;
						}

						switch (scResources[scBufferShaderInputs[i][k]].resource.type) {
							case IMAGE_TEXTURE: { // TODO: Compute input dimensions
								glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
								glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
								continue;
							}

							case AUDIO_TEXTURE: {
								glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
								glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
								continue;
							}

							case VIDEO_TEXTURE: {
								glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
								glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
								continue;
							}

							case MIC_TEXTURE: {
								glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
								glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
								continue;
							}

							case WEB_TEXTURE: {
								glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
								glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
								continue;
							}

							case FRAME_BUFFER: { // Buffer size always match the viewport size
								
								// Bind texture
								// TODO: In parallel rendering use different CL_TEXTURE* to avoid data intersection
								// TODO: Support for Sampler3D, e.t.c.
								glActiveTexture(GL_TEXTURE0 + k);
								glBindTexture(GL_TEXTURE_2D, glBufferShaderFramebufferTextures[scResources[scBufferShaderInputs[i][k]].resource.buffer_id]);
								glUniform1i(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelUniforms[k]), k);

								// Width & Height 
								glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) glWidth, (GLfloat) glHeight, (GLfloat) 0);
								
								// Timestamp of previous buffer frame
								glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) scTimestamp);
								continue;
							}
						}
					}

					// Render Buffer i
					glUseProgram(glBufferShaderProgramIDs[i]);
					glBindVertexArray(glSquareVAO);
					glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
					glBindVertexArray(0);
					glFlush();
				}
			}

			// Render Main Shader
			glClearColor(0, 0, 0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT);

			// Load all Basic inputs
			glUniform3f(glGetUniformLocation(glMainShaderProgramID, "iResolution"), (float) glWidth, (float) glHeight, 0.0);
			glUniform1f(glGetUniformLocation(glMainShaderProgramID, "iTime"), (float) glfwGetTime());
			glUniform1f(glGetUniformLocation(glMainShaderProgramID, "iTimeDelta"), (float) (glfwGetTime() - scTimestamp));
			glUniform1i(glGetUniformLocation(glMainShaderProgramID, "iFrame"), scFrames);

			// imouse.xy = current mouse location
			// iMouse.zw = previous mouse location
			// TODO: Validate iMouse.zw data
			if (scMouseEnabled)
				glUniform4f(glGetUniformLocation(glMainShaderProgramID, "iMouse"), (GLfloat) currentMouse.x, (GLfloat) currentMouse.y, (GLfloat) scMouse.x, (GLfloat) scMouse.y);
			else
				glUniform4f(glGetUniformLocation(glMainShaderProgramID, "iMouse"), (GLfloat) scMouse.x, (GLfloat) scMouse.y, 0, 0);

			glUniform4f(glGetUniformLocation(glMainShaderProgramID, "iDate"), (GLfloat) iDate_year, (GLfloat) iDate_month, (GLfloat) iDate_day, (GLfloat) iDate_time);

			// Default value for SampleRate
			// TODO: Should evaluate from inputs?
			glUniform1f(glGetUniformLocation(glMainShaderProgramID, "iSampleRate"), 0);

			// Bind iChannel data
			for (int k = 0; k < 4; ++k) {
				if (scMainShaderInputs[k] == -1) {
					glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
					glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
					continue;
				}

				if (scResources[scMainShaderInputs[k]].empty) {
					std::wcout << "Can not configure iChannelResolution for input " << k << " in Main Shader, input points to empty resource, scResources corrupt" << std::endl;
					glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
					glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
					continue;
				}

				switch (scResources[scMainShaderInputs[k]].resource.type) {
					case IMAGE_TEXTURE: { // TODO: Compute input dimensions
						glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
						glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
						continue;
					}

					case AUDIO_TEXTURE: {
						glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
						glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
						continue;
					}

					case VIDEO_TEXTURE: {
						glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
						glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
						continue;
					}

					case MIC_TEXTURE: {
						glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
						glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
						continue;
					}

					case WEB_TEXTURE: {
						glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
						glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
						continue;
					}

					case FRAME_BUFFER: { // Buffer size always match the viewport size

						// Bind texture
						// TODO: In parallel rendering use different CL_TEXTURE* to avoid data intersection
						// TODO: Support for Sampler3D, e.t.c.
						glActiveTexture(GL_TEXTURE0 + k);
						glBindTexture(GL_TEXTURE_2D, glBufferShaderFramebufferTextures[scResources[scMainShaderInputs[k]].resource.buffer_id]);
						glUniform1i(glGetUniformLocation(glMainShaderProgramID, iChannelUniforms[k]), k);

						// Width & Height 
						glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) glWidth, (GLfloat) glHeight, (GLfloat) 0);

						// Timestamp of previous buffer frame
						glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) scTimestamp);
						continue;
					}
				}
			}

			// Render Main Shader
			glUseProgram(glMainShaderProgramID);
			glBindVertexArray(glSquareVAO);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);

			glFlush();

			SwapBuffers(glDevice);

			// Update required values
			scTimestamp = (float) glfwGetTime();
			++scFrames;

			// Update mouse location
			scMouse.x = currentMouse.x;
			scMouse.y = currentMouse.y;
		}
	}
}


// Starts thread for rendering scene, synchronizes with main thread for careful event processing and resource rebinding
void startRenderThread() {
	renderThread = new std::thread([] () {

		// Just timers
		std::chrono::system_clock::time_point a = std::chrono::system_clock::now();
		std::chrono::system_clock::time_point b = std::chrono::system_clock::now();

		// Lock motex to avoid interruption when reloading resources or exiting
		renderMutex.lock();
		
		// Acquire context
		wglMakeCurrent(glDevice, glContext);

		// Yes, while true, i don't care
		while (true) {

			// To prevent mutex lock / unlock 100500 times a second, check if lock was requested by simply 
			//  cheking bool and after that try to relock
			if (appLockRequested) {

				// if lock was requested, release GL context
				wglMakeCurrent(NULL, NULL);

				// Unlock mutex and allow main thread to load texture / rebind shader / e.t.c
				renderMutex.unlock();
				// appLockRequested = FALSE; // TODO: Main thread should clear this flag manually
				
				// Wait 50ms else it will run into undefined race condilion self-lock
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				
				// Now main thread acquires lock
				renderMutex.lock();

				// Acquire context back
				wglMakeCurrent(glDevice, glContext);
			}

			// Check if render exit was requested
			if (appExiting) {

				// Stop render process and thread
				wglMakeCurrent(NULL, NULL);
				renderMutex.unlock();
				return;

			} else if (scPaused) {
				std::this_thread::sleep_for(std::chrono::milliseconds(scMinFrameTime));
			} else{
				a = std::chrono::system_clock::now();

				// One frame render
				static PAINTSTRUCT ps;
				BeginPaint(glWindow, &ps);
				renderSC();
				EndPaint(glWindow, &ps);

				b = std::chrono::system_clock::now();
				auto work_time = std::chrono::duration_cast<std::chrono::milliseconds>(a - b);

				if (work_time.count() < scMinFrameTime)
					std::this_thread::sleep_for(std::chrono::milliseconds(scMinFrameTime - work_time.count()));
			}
		}
	});
}


// Dispose all resources
void dispose() {
	
	wglMakeCurrent(glDevice, glContext);

	// Unlink all shaders & buffers
	unloadMainShader();

	for (int i = 0; i < 4; ++i) 
		if (glBufferShaderProgramIDs[i] != -1)
			glDeleteProgram(glBufferShaderProgramIDs[i]);

	// Buffer i buffer & texture
	glDeleteTextures(4, glBufferShaderFramebufferTextures);
	glDeleteBuffers(4, glBufferShaderFramebuffers);

	// Square buffer
	glDeleteVertexArrays(1, &glSquareVAO);
	glDeleteBuffers(1, &glSquareVBO);
	glDeleteBuffers(1, &glSquareEBO);

	// Unlink all resources
	unloadResources();

	wglMakeCurrent(NULL, NULL);
}


// Carefull stop application with dispose of all resources, threads and exit
void exitApp() {

	// Wait for rendering thread to exit
	appExiting = TRUE;
	renderThread->join();
	delete renderThread;

	// Tray icon delete
	Shell_NotifyIcon(NIM_DELETE, &trayNID);
	
	// TODO: Menu dispose?
	// ...

	// Resource dispose
	dispose();

	// Windows dispose (rm -rf /)
	wglMakeCurrent(NULL, NULL);
	ReleaseDC(glWindow, glDevice);
	wglDeleteContext(glContext);
	DestroyWindow(trayWindow);
	DestroyWindow(glWindow);

	if (glPalette)
		DeleteObject(glPalette);
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
LRESULT CALLBACK trayWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { // TODO: Accelerators &
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
					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_NEW, _T("New")); // TODO: Do we need it?
					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 0"));
					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 1"));
					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 2"));
					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, _T("Input 3"));

					// TODO: Create new submenu for each buffer / shader and add short info about input type
					trayMainBufferAShaderMenu = CreatePopupMenu();
					InsertMenu(trayMainBufferAShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_OPEN, _T("Open"));
					InsertMenu(trayMainBufferAShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_NEW, _T("New")); // TODO: Do we need it?
					InsertMenu(trayMainBufferAShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDS_SHADER_REMOVE, _T("Remove")); // TODO: Change to "Enabled" and "Paused"? because if it is paused, it is not evaluated on each frame and if it is not enabled, it is removed from pack
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

				default:
					return DefWindowProc(hWnd, uMsg, wParam, lParam);
			}
			break;
		}
		
		case WM_COMMAND: {
			wmId = LOWORD(wParam);
			wmEvent = HIWORD(wParam);

			switch (wmId) {
				case ID_SYSTRAYMENU_EXIT: {
					PostQuitMessage(0);
					return TRUE;
				}

				case ID_SYSTRAYMENU_PAUSE: {
					if (scPaused) {
						appLockRequested = TRUE;
						renderMutex.lock();

						// Restore timestamp
						wglMakeCurrent(glDevice, glContext);
						glfwSetTime(scTimestamp);
						wglMakeCurrent(NULL, NULL);

						appLockRequested = FALSE;
						renderMutex.unlock();
					}

					scPaused = !scPaused;
					return TRUE;
				}

				case ID_SYSTRAYMENU_DEBUG_WARNINGS: {
					glDebugOutput = !glDebugOutput;
					return TRUE;
				}

				case ID_SYSTRAYMENU_RESET_TIME: {
					appLockRequested = TRUE;
					renderMutex.lock();

					wglMakeCurrent(glDevice, glContext);
					glfwSetTime(0.0);
					scTimestamp = 0.0;
					scFrames = 0;
					wglMakeCurrent(NULL, NULL);

					appLockRequested = FALSE;
					renderMutex.unlock();
					return TRUE;
				}

				case ID_SYSTRAYMENU_MOUSE_MODE: {
					appLockRequested = TRUE;
					renderMutex.lock();
					
					scMouseEnabled = !scMouseEnabled;

					appLockRequested = FALSE;
					renderMutex.unlock();
					return TRUE;
				}

				case ID_SYSTRAYMENU_SOUND_MODE: {
					appLockRequested = TRUE;
					renderMutex.lock();

					scSoundEnabled = !scSoundEnabled;

					appLockRequested = FALSE;
					renderMutex.unlock();
					return TRUE;
				}

				// There could be more beautiful code, but i was lazy in this menu entry
				case ID_SYSTRAYMENU_FPS_SUBMENU + 1: { // 1
					scMinFrameTime = 1000;
					scFPSMode = 1;
					return TRUE;
				}

				case ID_SYSTRAYMENU_FPS_SUBMENU + 2: { // 15
					scMinFrameTime = 1000 / 15;
					scFPSMode = 15;
					return TRUE;
				}

				case ID_SYSTRAYMENU_FPS_SUBMENU + 3: { // 30
					scMinFrameTime = 1000 / 30;
					scFPSMode = 30;
					return TRUE;
				}

				case ID_SYSTRAYMENU_FPS_SUBMENU + 4: { // 60
					scMinFrameTime = 1000 / 60;
					scFPSMode = 60;
					return TRUE;
				}

				case ID_SYSTRAYMENU_FPS_SUBMENU + 5: { // 120
					scMinFrameTime = 1000 / 120;
					scFPSMode = 120;
					return TRUE;
				}

				case ID_SYSTRAYMENU_FULLSCREEN: {
					appLockRequested = TRUE;
					renderMutex.lock();

					if (scFullscreen) {

						// Rescan displays because user may reconnect them
						enumerateDisplays();

						// Check if there is > 0 displays
						if (displays.size() == 0) {
							MessageBoxA(
								NULL,
								"Can not display shader decause no displays found on the system",
								"Display not found",
								MB_ICONERROR | MB_OK
							);

							appLockRequested = FALSE;
							renderMutex.unlock();

							return TRUE;
						}

						// Ensure no out of bounds
						if (scDisplayID >= displays.size())
							scDisplayID = displays.size() - 1;

						currentWindowDimensions = displays[scDisplayID];
						
						// Window size & location
						MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

						// GL size
						glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
						glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

						wglMakeCurrent(glDevice, glContext);
						resizeSC();
						wglMakeCurrent(NULL, NULL);
						
						// Update flag (obviously)
						scFullscreen = FALSE;

					} else {

						// Rescan displays because user may reconnect them
						enumerateDisplays();

						// Check if there is > 0 displays
						if (displays.size() == 0) {
							MessageBoxA(
								NULL,
								"Can not display shader decause no displays found on the system",
								"Display not found",
								MB_ICONERROR | MB_OK
							);

							appLockRequested = FALSE;
							renderMutex.unlock();

							return TRUE;
						}

						currentWindowDimensions = fullViewportSize;

						// Window size & location
						MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

						// GL size
						glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
						glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

						wglMakeCurrent(glDevice, glContext);
						resizeSC();
						wglMakeCurrent(NULL, NULL);

						// Update flag (obviously)
						scFullscreen = TRUE;
					}

					appLockRequested = FALSE;
					renderMutex.unlock();
					return TRUE;
				}

				case ID_SYSTRAYMENU_MOVETONEXTDISPLAY: {
					// Save me from a little duck constantly watching me
					appLockRequested = TRUE;
					renderMutex.lock();

					if (scFullscreen) {

						// Always return to the same display
						// Rescan displays because user may reconnect them
						enumerateDisplays();

						// Check if there is > 0 displays
						if (displays.size() == 0) {
							MessageBoxA(
								NULL,
								"Can not display shader decause no displays found on the system",
								"Display not found",
								MB_ICONERROR | MB_OK
							);

							appLockRequested = FALSE;
							renderMutex.unlock();

							return TRUE;
						}

						// Ensure no out of bounds
						if (scDisplayID >= displays.size())
							scDisplayID = displays.size() - 1;

						currentWindowDimensions = displays[scDisplayID];

						// Window size & location
						MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

						// GL size
						glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
						glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

						wglMakeCurrent(glDevice, glContext);
						resizeSC();
						wglMakeCurrent(NULL, NULL);

						// Update flag (obviously)
						scFullscreen = FALSE;

					} else {

						// Rescan displays because user may reconnect them
						enumerateDisplays();

						// Shift display
						++scDisplayID;

						// Check if there is > 0 displays
						if (displays.size() == 0) {
							MessageBoxA(
								NULL,
								"Can not display shader decause no displays found on the system",
								"Display not found",
								MB_ICONERROR | MB_OK
							);

							appLockRequested = FALSE;
							renderMutex.unlock();

							return TRUE;
						}

						// Ensure no out of bounds, loop displays
						if (scDisplayID >= displays.size())
							scDisplayID = 0;

						currentWindowDimensions = displays[scDisplayID];

						// Window size & location
						MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

						// GL size
						glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
						glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

						wglMakeCurrent(glDevice, glContext);
						resizeSC();
						wglMakeCurrent(NULL, NULL);

						// Update flag (obviously)
						scFullscreen = FALSE;
					}

					appLockRequested = FALSE;
					renderMutex.unlock();
					return TRUE;
				}

				case ID_SYSTRAYMENU_MOVETOPREVDISPLAY: {
					// Save me from a little duck constantly watching me
					appLockRequested = TRUE;
					renderMutex.lock();

					if (scFullscreen) {

						// Always return to the same display
						// Rescan displays because user may reconnect them
						enumerateDisplays();

						// Check if there is > 0 displays
						if (displays.size() == 0) {
							MessageBoxA(
								NULL,
								"Can not display shader decause no displays found on the system",
								"Display not found",
								MB_ICONERROR | MB_OK
							);

							appLockRequested = FALSE;
							renderMutex.unlock();

							return TRUE;
						}

						// Ensure no out of bounds
						if (scDisplayID >= displays.size())
							scDisplayID = displays.size() - 1;

						currentWindowDimensions = displays[scDisplayID];

						// Window size & location
						MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

						// GL size
						glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
						glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

						wglMakeCurrent(glDevice, glContext);
						resizeSC();
						wglMakeCurrent(NULL, NULL);

						// Update flag (obviously)
						scFullscreen = FALSE;

					} else {

						// Rescan displays because user may reconnect them
						enumerateDisplays();

						// Shift display
						--scDisplayID;

						// Check if there is > 0 displays
						if (displays.size() == 0) {
							MessageBoxA(
								NULL,
								"Can not display shader decause no displays found on the system",
								"Display not found",
								MB_ICONERROR | MB_OK
							);

							appLockRequested = FALSE;
							renderMutex.unlock();

							return TRUE;
						}

						// Ensure no out of bounds, loop displays
						if (scDisplayID < 0)
							scDisplayID = displays.size() - 1;
						
						if (scDisplayID >= displays.size())
							scDisplayID = displays.size() - 1;

						currentWindowDimensions = displays[scDisplayID];

						// Window size & location
						MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

						// GL size
						glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
						glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

						wglMakeCurrent(glDevice, glContext);
						resizeSC();
						wglMakeCurrent(NULL, NULL);

						// Update flag (obviously)
						scFullscreen = FALSE;
					}

					appLockRequested = FALSE;
					renderMutex.unlock();
					return TRUE;
				}

				case ID_SYSTRAYMENU_RESCAN_DISPLAYS: {
					appLockRequested = TRUE;
					renderMutex.lock();

					// Rescan displays because user may reconnect them
					enumerateDisplays();

					// Check if there is > 0 displays
					if (displays.size() == 0) {
						MessageBoxA(
							NULL,
							"Can not display shader decause no displays found on the system",
							"Display not found",
							MB_ICONERROR | MB_OK
						);

						appLockRequested = FALSE;
						renderMutex.unlock();

						return TRUE;
					}

					if (scFullscreen) {

						currentWindowDimensions = fullViewportSize;

						// Window size & location
						MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

						// GL size
						int nglWidth = currentWindowDimensions.right - currentWindowDimensions.left;
						int nglHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

						// Resize context and textures only if there is a size mismatch
						if (nglWidth != glWidth || nglHeight != glHeight) {
							glWidth = nglWidth;
							glHeight = nglHeight;

							wglMakeCurrent(glDevice, glContext);
							resizeSC();
							wglMakeCurrent(NULL, NULL);
						}

					} else {

						// Ensure no out of bounds, loop displays
						if (scDisplayID >= displays.size())
							scDisplayID = 0;

						currentWindowDimensions = displays[scDisplayID];

						// Window size & location
						MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

						// GL size
						int nglWidth = currentWindowDimensions.right - currentWindowDimensions.left;
						int nglHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

						// Resize context and textures only if there is a size mismatch
						if (nglWidth != glWidth || nglHeight != glHeight) {
							glWidth = nglWidth;
							glHeight = nglHeight;

							wglMakeCurrent(glDevice, glContext);
							resizeSC();
							wglMakeCurrent(NULL, NULL);
						}
					}

					appLockRequested = FALSE;
					renderMutex.unlock();
					return TRUE;
				}
				
				default:
					return DefWindowProc(hWnd, wmId, wParam, lParam);
			}
		}
		
		case WM_DESTROY: {
			PostQuitMessage(0);
			return TRUE;
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
	wcex.lpfnWndProc = (WNDPROC) trayWindowProc;
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
LRESULT CALLBACK desktopWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		//case WM_PAINT: {
		//	// Prevent ignoring of paint event because GL implicitly forces window to repaint
		//	return TRUE;
		//}

		case WM_SIZE: {
			// Do nothing, resized on menu item click inside mutex lock context
			return TRUE;
		}

		// IDK what it does, lol
		case WM_PALETTECHANGED:
			if (hWnd == (HWND) wParam)
				break;

		case WM_QUERYNEWPALETTE:
			if (glPalette) {
				UnrealizeObject(glPalette);
				SelectPalette(glDevice, glPalette, FALSE);
				RealizePalette(glDevice);
				return TRUE;
			}
			return TRUE;

		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
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

	// This cake is not real
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

	MSG msg;

	while (1) {
		// Block till message received
		if (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else
			break;

		if (msg.message == WM_QUIT)
			break;
	}
}


// Entry
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow) {

	//UNREFERENCED_PARAMETER(hPrevInstance);
	//UNREFERENCED_PARAMETER(pCmdLine);

	// Debug: Display console window
#ifdef DEBUG_DISPLAY_CONSOLE_WINDOW
	// TODO: Commandline parameter to open with console
	// TODO: Prevent console closing exiting app https://stackoverflow.com/questions/20232685/how-can-i-prevent-my-program-from-closing-when-a-open-console-window-is-closed
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
	wglMakeCurrent(glDevice, glContext);

	initSC();

	ShowWindow(glWindow, nCmdShow);
	UpdateWindow(glWindow);

	// DEBUG: Load sample shader and run
#ifdef DEBUG_LOAD_SHADER_FROM_FILE
	glDebugOutput = TRUE;

	ShaderCompilationStatus compilationStatus = compileShaderFromFile("shader.glsl"); // compileShader(defaultShader); // compileShaderFromFile("shader.glsl");

	if (!compilationStatus.success) {
		std::wcout << "You're debugging the thing that doesn't work" << std::endl;
		system("PAUSE");
	}  else
		glMainShaderProgramID = compilationStatus.shaderID;

	scMouseEnabled = TRUE;
	// scPaused = TRUE;

	// Move to second display
	currentWindowDimensions = displays[scDisplayID = 1];

	// Window size & location
	MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

	// GL size
	glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
	glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

#endif

	// Release OpenGL context for this thread
	wglMakeCurrent(NULL, NULL);

	// Start new thread for render
	startRenderThread();

	// Enter message dispatching loop
	enterDispatchLoop();

	// After dispatch loop: exit and dispose
	exitApp();

	return 0;
}

